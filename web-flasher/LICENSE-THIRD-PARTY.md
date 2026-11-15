# Third-party software

`lib/flasher.js`, `test/flasher.test.mjs`, and the X3 installation sequence derive
from crosspoint-reader/crosspoint-tools at commit
`b62c8b57880415bfc73ad5a5941cc8087846d256`.
`lib/esptool.bundle.js` is copied unchanged from that source (vendored esptool-js;
its embedded dependency notices are retained). esptool-js is Copyright Espressif
Systems and licensed under the Apache License 2.0
(<https://github.com/espressif/esptool-js>); the full text is in
[LICENSE-Apache-2.0.txt](./LICENSE-Apache-2.0.txt).

The firmware files offered by this page have their own license files
(`LICENSE-firmware*.txt`, GPLv3 for the combined firmware). The reading fonts in
`wren-fonts.zip` are under the SIL Open Font License 1.1; each font folder carries
its `OFL.txt`.

The page itself uses Literata and IBM Plex Mono web fonts (`assets/fonts/`, from the
Fontsource 5.1.0 packages), both under the SIL Open Font License 1.1. The license
texts are `assets/fonts/OFL-literata.txt` and `assets/fonts/OFL-ibm-plex-mono.txt`.

The installer code below keeps the crosspoint-tools MIT notice:

MIT License

Copyright (c) 2025 SoFriendly

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
