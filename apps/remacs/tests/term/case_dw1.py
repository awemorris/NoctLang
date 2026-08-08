KEYS = [
    (0.4, b"abc"),
    (0.2, b"\x182"),   # C-x 2
    (0.2, b"\x180"),   # C-x 0 (delete selected=top)
    (0.3, b"xyz"),
    (0.3, b"\x18\x03"),
]
EXPECT = ["abcxyz"]
