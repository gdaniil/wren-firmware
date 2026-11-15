# Wren web flasher: copy

All lowercase. Headlines max 22 characters, sublines max 70. No em dashes. Hero title kept.

## Hero

| id | Daniil's draft | proposed | why |
|---|---|---|---|
| hero.title | hey, wren | hey, wren | already the voice of the page; alt: "meet wren" (flatter, not better) |
| hero.sub | crosspoint-based firmware, redesigned for reading | a quieter way to read on your xteink | says who it is for and the benefit; "crosspoint" moves to the footer, geeks find it there |

## Features

| id | Daniil's draft | proposed | why |
|---|---|---|---|
| f1.head | out of box reading | books, ready to read | "out of box" is jargon; says what you get |
| f1.sub | reading experience optimized so you don't have to think | good type and spacing, set before you open a book | concrete: the work is already done |
| f2.head | setting simplified | fewer settings | plain, true, 2 words |
| f2.sub | settings simpllified for readers not for geeks | font, size, alignment, orientation. that's it | names the four real settings; the list is the argument |
| f3.head | better navigation | your library is home | the screen is the point, not "navigation" |
| f3.sub | yourhome screen is you library, the only screen you need 99% of time | open the reader, pick up your book. the rest is two tabs away | benefit first; "99%" is an invented number, dropped |
| f4.head | same transfering | send books your way | keeps the promise without "same" |
| f4.sub | sd card, hotspot, wifi, calibre - everything you used to in crosspoint | wi-fi, hotspot, calibre or sd card. nothing new to learn | all four routes are real (see notes) |
| f5.head | just, designed | familiar, but nicer | the comma in "just, designed" reads as a typo |
| f5.sub | crosspoint-based, falimiar, but nicer | built on crosspoint, so it works with the apps you have | turns "based on" into a benefit: existing apps keep working |

### Alternatives

| id | alternative head | alternative sub |
|---|---|---|
| f1 | open, read | clean pages, no setup before page one |
| f2 | settings for readers | only what changes how a book looks |
| f3 | home is your library | your current book on top, the rest below |
| f4 | books in, any way | from your phone, your computer or a card |
| f5 | crosspoint, polished | same trusted base, drawn with more care |

## Installer

| id | Daniil's draft | proposed | why |
|---|---|---|---|
| unsupported.text | ahh... please, use chrome to install wren | ahh... please, use chrome to install wren | keep as is, it is the page's best line |
| unsupported.btn | get chrome | get chrome | fine; edge also works, mention under the button: "or edge" |
| phone.text | oh...install wren using chrome browser on your computer | oh... open this page in chrome on your computer | a phone cannot flash; says where to go |
| step1.text | connect your reader to computer via cable and wake it up | plug in your reader and wake it up | shorter; "wake it up" is the step people miss |
| step1.hint | (none) | use a data cable. the 2-pin x3 cable only charges | real failure cause (app.js cableHint); safety text, no joke |
| step1.btn | connect reader | connect reader | fine |
| step2.text | choose your device and wake up your reader | which reader do you have? | question, one job; waking moved to step 1 |
| step2.options | X3/X4, X4pro, X4classic | x3 or x4, x4 pro, x4 classic | matches real model names and the real radio groups |
| step2.note | (none) | not tested on this model yet | true for x4 classic and x4 pro; only we own an x3 |
| step2.btn | install wren | continue | install belongs after the backup step |
| backup.text | (missing) | keep a copy of your reader first | benefit-led, plain |
| backup.hint | (missing) | this saves the full 16 mb to your computer. keep the file somewhere safe | matches the real flow |
| backup.btn | (missing) | save a backup | verb phrase |
| backup.check | (missing) | i already have a backup | matches the real checkbox |
| install.btn | install wren | install wren | fine |
| install.text | installing wren... keep the cable connected | installing wren... keep the cable connected | keep; the cable warning is the key info |
| install.percent | 22% | 22% | unchanged |
| done.head | ready to read! | ready to read | drop the exclamation mark |
| done.text | unplug the cable and restart your reader. | unplug the cable, then press the power button to restart | matches app.js; the reader restarts by hand |
| done.fonts | download, unzip and put fonts folder to the root of your sd card for all fonts options. | for literata, golos and bitter, unzip fonts.zip and copy the fonts folder to your sd card | names the benefit; without it the reader has literata in 3 sizes |
| done.btn | fonts.zip | download fonts | verb phrase; size can follow ("3.2 mb", from firmware.json) |
| error.head | (missing) | connection ended | already in app.js; plain on purpose |
| error.text | (missing) | unplug the cable, wake the reader, plug it back in and try again. | no humor in errors |
| error.btn | (missing) | try again | verb phrase |
| error.locked | (missing) | this reader is locked, so wren cannot be installed. | matches the app.js "locked" message; do not soften |

## Notes

- Calibre: found in the repo. "calibre wireless" is a row in the send menu (`src/wren/send/SendState.h`, `docs/ours/briefs/N1_SEND.md`); it uses the upstream Calibre screen. Claim kept. I did not run it on a device.
- "same as crosspoint" for transfer is true for the built-in web page and the existing apps (Send to X4, CrossPoint Sync) per the project brief. Android apps are early; not named in copy. The hotspot differs: it is named `wren-xxxx` and open (REPO_MAP section 6), so the copy says "hotspot" and nothing about its name.
- Dropped: "99% of the time" (no data); "optimized" (no measured claim); "ready to read!" exclamation.
- Settings list (font, size, alignment, orientation): the setup plan lists reader and display sections, but I did not verify the exact four items on screen. Check against the screenshot before shipping.
- Feature 1: "chapter 1 of Moby-Dick" screenshot and "set before you open a book" rest on the typography spec (Literata, Golos, Bitter). Not tested on a device.
- Feature 5 end-of-book screen: I did not find the screen in the plans I skimmed. Verify "start over" and "delete from card" exist before the screenshot is used.
- Install steps now read: connect, choose reader, backup, install. The draft has choose-reader as step 2; the real flow (index.html) shows the model choice before connecting. Flow order should match the page, not the draft.
- Only the x3 is owned. X4, x4 classic, x4 pro installs are unverified; the "not tested" note stays.
