#pragma once

#include <string>
#include <vector>
#include "Pattern.h"

/// @file Renderer.h
/// @brief Provides display functions for the REPL beat maker.

namespace Renderer
{
    void PrintStatus(const Pattern& pattern);
    void PrintHelp();
    void PrintBanner();
}
