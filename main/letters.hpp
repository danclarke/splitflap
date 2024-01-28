#pragma once

// Each letter / flap, in order for a unit
// Newer C++ should support UTF8 characters here
const char unitLetters[] = {' ', 'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z', '$', '&', '#', '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', ':', '.', '-', '?', '!'};

// Number of flaps
const int unitLettersCount = sizeof(unitLetters) / sizeof(unitLetters[0]);

// Index of 'A' within the unitLetters array
const int unitLettersLettersStart = 1;

// Index if '0' within the unitLetters array
const int unitLettersNumbersStart = 30;

// Letter numbers to use for calibration
// Space, Z, A, U, N, ?, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9
const int calLetters[] = {0, 26, 1, 21, 14, 43, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39};

// Number of letters in calLetters
const int calLettersCount = sizeof(calLetters) / sizeof(calLetters[0]);