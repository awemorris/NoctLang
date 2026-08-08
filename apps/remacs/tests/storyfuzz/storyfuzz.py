#!/usr/bin/env python3
"""Story fuzzing for remacs: exploratory system testing.

Coherent editor-operation "stories" (typing, isearch, SKK input,
window juggling, ...) are synthesized from a random seed, randomly
interleaved, and randomly aborted with C-g mid-story. Many seeded
sessions run in parallel; any session that crashes, trips
noct_error()/rt_error() (the CLI prints "file:line: Error: ..." on
stderr and exits nonzero), or hangs is reported with its seed and an
artifact directory for replay and debugging.

Usage:
  python3 storyfuzz.py --runs 200 --jobs 8        # explore
  python3 storyfuzz.py --seed 12345 --verbose     # reproduce one run

A run is deterministic in the byte stream it sends for a given seed
(timing jitter can in principle change paste coalescing, so a rare
failure may need a few replays to fire again).
"""

import argparse
import multiprocessing
import os
import pty
import random
import select
import shutil
import signal
import subprocess
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))
DEFAULT_REMACS = os.path.join(HERE, '..', '..', 'build-debug', 'remacs')
FAILURE_DIR = os.path.join(HERE, 'failures')

ESC = b'\x1b'
CTRL_G = b'\x07'

WORDS = ['alpha', 'beta', 'gamma', 'delta', 'code', 'test', 'editor',
         'window', 'buffer', 'point', 'mark', 'kanji', 'remacs']


# ---------------------------------------------------------------------------
# Stories: each returns a list of (label, bytes) steps. The driver may
# abort after any step with C-g and move on to another story.
# ---------------------------------------------------------------------------

def story_type_text(rng):
    steps = []
    for _ in range(rng.randint(2, 6)):
        line = ' '.join(rng.choice(WORDS) for _ in range(rng.randint(1, 5)))
        steps.append(('type', line.encode()))
        if rng.random() < 0.7:
            steps.append(('newline', b'\r'))
    return steps


def story_navigate(rng):
    moves = [b'\x01', b'\x05', b'\x06', b'\x02', b'\x0e', b'\x10',   # C-a C-e C-f C-b C-n C-p
             ESC + b'<', ESC + b'>', ESC + b'f', ESC + b'b',
             b'\x16', ESC + b'v',                                    # C-v M-v
             ESC + b'[A', ESC + b'[B', ESC + b'[C', ESC + b'[D']
    return [('move', rng.choice(moves)) for _ in range(rng.randint(3, 10))]


def story_kill_yank(rng):
    steps = [('mark', b'\x00')]                                      # C-SPC
    for _ in range(rng.randint(1, 5)):
        steps.append(('move', rng.choice([b'\x06', b'\x0e', ESC + b'f'])))
    steps.append(('kill', rng.choice([b'\x17', ESC + b'w'])))        # C-w / M-w
    steps.append(('yank', b'\x19'))                                  # C-y
    for _ in range(rng.randint(0, 3)):
        steps.append(('yank-pop', ESC + b'y'))
    if rng.random() < 0.5:
        steps.append(('kill-line', b'\x0b' * rng.randint(1, 3)))     # C-k
    return steps


def story_edit_small(rng):
    picks = [b'\x04', b'\x7f', b'\x0f', b'\x14',                     # C-d DEL C-o C-t
             ESC + b'u', ESC + b'l', ESC + b'c',                     # case ops
             ESC + b'd', ESC + b'\x7f']                              # kill-word back-kill-word
    return [('edit', rng.choice(picks)) for _ in range(rng.randint(2, 8))]


def story_undo(rng):
    return [('undo', b'\x1f') for _ in range(rng.randint(1, 8))]     # C-_


def story_isearch(rng):
    d = rng.choice([b'\x13', b'\x12'])                               # C-s / C-r
    steps = [('isearch', d)]
    word = rng.choice(WORDS)
    for ch in word[:rng.randint(1, len(word))]:
        steps.append(('isearch-key', ch.encode()))
    for _ in range(rng.randint(0, 3)):
        steps.append(('isearch-next', d))
    steps.append(('isearch-end', rng.choice([b'\r', CTRL_G])))
    return steps


def story_goto_line(rng):
    return [('goto', ESC + b'gg'),
            ('goto-num', str(rng.randint(0, 99)).encode()),
            ('goto-ret', b'\r')]


def story_prefix_arg(rng):
    n = str(rng.randint(1, 12)).encode()
    return [('C-u', b'\x15'), ('num', n),
            ('cmd', rng.choice([b'\x06', b'\x0e', b'\x04', b'\x0b']))]


def story_find_file(rng):
    name = rng.choice(['sample.txt', 'other.txt', 'nofile-%d.txt'
                       % rng.randint(0, 999)])
    return [('find-file', b'\x18\x06'),
            ('ff-name', name.encode()),
            ('ff-ret', rng.choice([b'\r', CTRL_G]))]


def story_buffers(rng):
    name = rng.choice(['*scratch*', 'buf-a', 'buf-b'])
    steps = [('C-x b', b'\x18b'), ('buf-name', name.encode()),
             ('buf-ret', rng.choice([b'\r', CTRL_G]))]
    if rng.random() < 0.4:
        steps.append(('kill-buf', b'\x18k'))
        steps.append(('kill-ret', rng.choice([b'\r', CTRL_G])))
    return steps


def story_windows(rng):
    picks = [b'\x182', b'\x183', b'\x18o', b'\x181', b'\x180']
    return [('win', rng.choice(picks)) for _ in range(rng.randint(2, 6))]


def story_save(rng):
    steps = [('save', b'\x18\x13')]                                  # C-x C-s
    # A file-less buffer prompts for the name.
    steps.append(('save-name', ('save-%d.txt' % rng.randint(0, 99)).encode()))
    steps.append(('save-ret', rng.choice([b'\r', CTRL_G])))
    return steps


def story_query_replace(rng):
    frm, to = rng.sample(WORDS, 2)
    steps = [('M-%', ESC + b'%'),
             ('from', frm.encode()), ('ret', b'\r'),
             ('to', to.encode()), ('ret2', b'\r')]
    for _ in range(rng.randint(1, 5)):
        steps.append(('answer', rng.choice([b'y', b'n', b'!', b'q', b'.'])))
    return steps


def story_mx(rng):
    cmd = rng.choice(['forward-word', 'backward-word', 'undo', 'recenter',
                      'beginning-of-buffer', 'end-of-buffer',
                      'split-window-vertically', 'delete-other-windows'])
    return [('M-x', ESC + b'x'), ('mx-name', cmd.encode()),
            ('mx-ret', rng.choice([b'\r', CTRL_G]))]


def story_skk(rng):
    steps = [('skk-on', b'\x18\x0a')]                                # C-x C-j
    for _ in range(rng.randint(1, 4)):
        kind = rng.random()
        if kind < 0.4:
            # Plain kana typing.
            steps.append(('kana', rng.choice(
                [b'kanji', b'aiueo', b'konnnitiha', b'sakura'])))
        elif kind < 0.8:
            # Conversion: capitalized reading, then SPC / cycle / decide.
            steps.append(('conv-start', rng.choice([b'Kanji', b'Ka', b'Aa'])))
            for _ in range(rng.randint(1, 3)):
                steps.append(('conv-spc', b' '))
            steps.append(('conv-end', rng.choice([b'\r', CTRL_G, b'x'])))
        else:
            # Katakana toggle / latin escape.
            steps.append(('skk-q', b'q'))
            steps.append(('kana2', rng.choice([b'neko', b'inu'])))
    steps.append(('skk-off', b'\x18\x0a'))
    return steps


def story_paste_burst(rng):
    lines = []
    for _ in range(rng.randint(5, 40)):
        lines.append(' '.join(rng.choice(WORDS)
                              for _ in range(rng.randint(1, 8))))
    return [('paste', ('\r'.join(lines)).encode())]


STORIES = [
    (story_type_text, 3), (story_navigate, 3), (story_kill_yank, 2),
    (story_edit_small, 2), (story_undo, 1), (story_isearch, 2),
    (story_goto_line, 1), (story_prefix_arg, 1), (story_find_file, 2),
    (story_buffers, 2), (story_windows, 2), (story_save, 1),
    (story_query_replace, 1), (story_mx, 1), (story_skk, 2),
    (story_paste_burst, 1),
]


def pick_story(rng):
    total = sum(w for _, w in STORIES)
    r = rng.uniform(0, total)
    for fn, w in STORIES:
        r -= w
        if r <= 0:
            return fn
    return STORIES[0][0]


# ---------------------------------------------------------------------------
# Driver
# ---------------------------------------------------------------------------

def make_home(base, seed):
    home = os.path.join(base, 'home-%d' % seed)
    shutil.rmtree(home, ignore_errors=True)
    os.makedirs(home)
    dic = ';; okuri-nasi\n' \
          'かんじ /漢字/感じ/\nか /可/課/\nああ /嗚呼/\nねこ /猫/\nいぬ /犬/\n'
    with open(os.path.join(home, 'SKK-JISYO.L'), 'wb') as f:
        f.write(dic.encode('euc_jp'))
    with open(os.path.join(home, 'sample.txt'), 'w') as f:
        f.write('sample file for find-file\nalpha beta gamma\n')
    with open(os.path.join(home, 'other.txt'), 'w') as f:
        f.write('other file\n')
    return home


def run_one(seed, remacs, n_stories, abort_p, time_limit, verbose=False):
    rng = random.Random(seed)
    base = '/tmp/remacs-storyfuzz'
    os.makedirs(base, exist_ok=True)
    home = make_home(base, seed)

    master, slave = pty.openpty()
    import fcntl, struct, termios
    fcntl.ioctl(slave, termios.TIOCSWINSZ, struct.pack('HHHH', 24, 80, 0, 0))
    env = dict(os.environ)
    env['HOME'] = home
    err_path = os.path.join(home, 'stderr.txt')
    err_f = open(err_path, 'wb')
    proc = subprocess.Popen([remacs], stdin=slave, stdout=slave,
                            stderr=err_f, close_fds=True, env=env, cwd=home)
    os.close(slave)

    raw = b''
    keylog = []

    def pump(sec):
        nonlocal raw
        end = time.time() + sec
        alive = True
        while time.time() < end:
            r, _, _ = select.select([master], [], [], 0.02)
            if master in r:
                try:
                    chunk = os.read(master, 8192)
                except OSError:
                    alive = False
                    break
                if not chunk:
                    alive = False
                    break
                raw += chunk
        return alive

    def send(label, data):
        keylog.append((label, data))
        if verbose:
            print('  %-14s %r' % (label, data))
        try:
            os.write(master, data)
        except OSError:
            return False
        return pump(rng.uniform(0.02, 0.06))

    deadline = time.time() + time_limit
    pump(0.6)                       # initial frame
    ok_io = True
    for si in range(n_stories):
        if time.time() > deadline or not ok_io or proc.poll() is not None:
            break
        fn = pick_story(rng)
        if verbose:
            print('[story %d] %s' % (si, fn.__name__))
        for label, data in fn(rng):
            ok_io = send(label, data)
            if not ok_io or proc.poll() is not None:
                break
            if rng.random() < abort_p:
                nquit = rng.randint(1, 2)
                ok_io = send('ABORT C-g', CTRL_G * nquit)
                break
            if time.time() > deadline:
                break

    # Orderly exit: escape any pending mode, then quit.
    result = 'OK'
    if proc.poll() is None:
        send('finish C-g', CTRL_G + CTRL_G)
        send('quit', b'\x18\x03')
        end = time.time() + 5
        while time.time() < end and proc.poll() is None:
            pump(0.1)
        if proc.poll() is None:
            result = 'HANG'
            proc.kill()
            proc.wait()

    rc = proc.returncode
    err_f.close()
    with open(err_path, 'rb') as f:
        err = f.read()
    os.close(master)

    if result != 'HANG':
        if rc is not None and rc < 0:
            result = 'CRASH(sig %d)' % -rc
        elif b'Error:' in err:
            result = 'RT_ERROR'
        elif rc != 0:
            result = 'EXIT(%s)' % rc

    if result == 'OK':
        shutil.rmtree(home, ignore_errors=True)
        return {'seed': seed, 'result': 'OK'}

    # Preserve artifacts for debugging.
    art = os.path.join(FAILURE_DIR, 'seed-%d' % seed)
    shutil.rmtree(art, ignore_errors=True)
    os.makedirs(art)
    with open(os.path.join(art, 'keylog.txt'), 'w') as f:
        f.write('seed %d  result %s  rc %s\n' % (seed, result, rc))
        f.write('replay: python3 storyfuzz.py --seed %d --verbose\n\n' % seed)
        for label, data in keylog:
            f.write('%-14s %r\n' % (label, data))
    with open(os.path.join(art, 'stderr.txt'), 'wb') as f:
        f.write(err)
    with open(os.path.join(art, 'screen-raw.txt'), 'wb') as f:
        f.write(raw[-8192:])
    return {'seed': seed, 'result': result,
            'stderr': err.decode('utf-8', 'replace').strip()[-300:],
            'artifacts': art}


def worker(args):
    seed, remacs, n_stories, abort_p, time_limit = args
    try:
        return run_one(seed, remacs, n_stories, abort_p, time_limit)
    except Exception as e:
        return {'seed': seed, 'result': 'HARNESS(%s)' % e}


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--remacs', default=DEFAULT_REMACS)
    ap.add_argument('--runs', type=int, default=100)
    ap.add_argument('--jobs', type=int, default=os.cpu_count() or 4)
    ap.add_argument('--seed', type=int, default=None,
                    help='run one seed (reproduce a failure)')
    ap.add_argument('--base-seed', type=int, default=None,
                    help='first seed of the sweep (default: random)')
    ap.add_argument('--stories', type=int, default=12,
                    help='stories per session')
    ap.add_argument('--abort-p', type=float, default=0.12,
                    help='per-step probability of a C-g story switch')
    ap.add_argument('--time-limit', type=float, default=25.0)
    ap.add_argument('--verbose', action='store_true')
    args = ap.parse_args()

    remacs = os.path.abspath(args.remacs)
    if not os.path.exists(remacs):
        print('remacs binary not found: %s' % remacs)
        return 2

    if args.seed is not None:
        r = run_one(args.seed, remacs, args.stories, args.abort_p,
                    args.time_limit, verbose=args.verbose)
        print(r)
        return 0 if r['result'] == 'OK' else 1

    base = args.base_seed
    if base is None:
        base = random.SystemRandom().randint(0, 10**9)
    print('story fuzz: %d runs, %d jobs, seeds %d..%d'
          % (args.runs, args.jobs, base, base + args.runs - 1))
    tasks = [(base + i, remacs, args.stories, args.abort_p, args.time_limit)
             for i in range(args.runs)]
    failures = []
    done = 0
    with multiprocessing.Pool(args.jobs) as pool:
        for r in pool.imap_unordered(worker, tasks):
            done += 1
            if r['result'] != 'OK':
                failures.append(r)
                print('FAIL seed=%d  %s' % (r['seed'], r['result']))
                if r.get('stderr'):
                    print('     stderr: %s' % r['stderr'])
            if done % 20 == 0:
                print('  ... %d/%d done, %d failures'
                      % (done, args.runs, len(failures)))

    print()
    if not failures:
        print('all %d runs OK' % args.runs)
        return 0
    print('%d/%d runs failed:' % (len(failures), args.runs))
    for r in sorted(failures, key=lambda x: x['seed']):
        print('  seed %-10d %-14s %s'
              % (r['seed'], r['result'], r.get('artifacts', '')))
    print('\nreproduce: python3 %s --seed <N> --verbose'
          % os.path.relpath(__file__))
    return 1


if __name__ == '__main__':
    sys.exit(main())
