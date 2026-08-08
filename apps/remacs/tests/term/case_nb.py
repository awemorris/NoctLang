# The whole editor as one 8.3 bytecode file on an unmodified noct:
#     noct REMACS.NB file
import subprocess, pathlib
root = pathlib.Path('/home/awe/NoctLang/samples/remacs')
subprocess.run(['sh', 'tools/build-nb.sh', 'build-debug/noctlang/noct',
                'build-debug/generated', 'build-nb'],
               cwd=root, check=True, capture_output=True)
pathlib.Path('/tmp/remacs-nb-test.txt').write_text('from bytecode\n')
ARGS = [str(root / 'build-nb' / 'REMACS.NB'), '/tmp/remacs-nb-test.txt']
KEYS = [
    (0.8, b"edited "),
    (0.3, b"\x18\x03"),
]
EXPECT = ["edited from bytecode", "remacs: remacs-nb-test.txt"]
