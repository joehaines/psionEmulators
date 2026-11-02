// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

#pragma once

// The characters with a key of their own on the Siena keyboard (besides
// letters, digits and space): the key and whether it needs Shift. Checked
// against the v4.20f ROM in Word. Shift on the digit keys still types the
// digit; everything else is typed with Ctrl + its three-digit code (see
// KeyPlayer::typeChar).
inline bool sienaChar(char c, int &key, bool &shift) {
    shift = false;
    switch (c) {
    case ';': case '/': case '-': case '=': case '+': case '*': case ',': case '.':
        key = c; return true;
    case ':': key = ';'; shift = true; return true;
    case '?': key = '/'; shift = true; return true;
    case '_': key = '-'; shift = true; return true;
    case '<': key = ','; shift = true; return true;
    case '>': key = '.'; shift = true; return true;
    }
    return false;
}
