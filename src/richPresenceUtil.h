#pragma once
#include "pch.h"
#include <stdio.h>
#include <sstream>
#include <ctime>
#include <algorithm>
#include <sstream>
#include "DRPWrap.h"
#include "gdapi.h"

#include <toml.hpp>
#include <fstream>

#include <fmt/format.h>

#ifndef RICHPRESENCEUTIL_H
#define RICHPRESENCEUTIL_H
enum class playerState
{
	level,
	editor,
	menu,
};

extern playerState currentPlayerState;
extern int *currentGameLevel;
extern bool updatePresence;
extern bool updateTimestamp;
extern bool editor_reset_timestamp;
void safeClose();
DWORD WINAPI mainThread(LPVOID lpParam);
#endif