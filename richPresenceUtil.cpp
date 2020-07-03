#include "pch.h"
#include "richPresenceUtil.h"

playerState currentPlayerState = playerState::menu;
int *currentGameLevel;
bool updatePresence;
bool updateTimestamp;
time_t currentTimestamp = time(0);

int *getBase(int pointer)
{
	return (int *)((int)GetModuleHandle(L"GeometryDash.exe") + pointer);
}

void updatePresenceS(std::string &details, std::string &largeText, std::string &smallText,
											std::string &state, std::string &smallImage)
{
	if (updateTimestamp)
	{
		currentTimestamp = time(0);
		updateTimestamp = false;
	}
	#ifdef _DEBUG
		std::stringstream ss;
		ss << "d: " << details << " s: " << state << "\nst: "<< smallText << " lt: " << largeText;
		#ifdef __MINGW32__
			MessageBoxA(0, ss.str().c_str(), "b", MB_OK);
		#else
			std::cout << ss.str() << std::endl;
		#endif
	#endif
	DRP::UpdatePresence(details.c_str(), largeText.c_str(), smallText.c_str(),
											state.c_str(), smallImage.c_str(), currentTimestamp);
}

void safeClose()
{
#ifdef _DEBUG
	SetConsoleTitleA("close called");
#endif
	Discord_Shutdown();
}

//insane_demon to Insane Demon
std::string getTextFromKey(std::string key)
{
	key[0] = toupper(key[0]); // uppercase first letter
	int index = 1;						// start from 1 because we start string from 1, and also because you can't get an index at -1 like it'll try to do..
	std::for_each(key.begin() + 1, key.end(), [&index, &key](char &letter) {
		// in any case, this checks if it's a space before
		if (key[index - 1] == ' ')
		{
			letter = toupper(letter); // then capitalizes
		}
		else if (letter == '_')
		{ // if underscore it goes and remoes it
			letter = ' ';
		}
		index++; // then we use this to count
	});
	return key;
}

std::string formatWithLevel(std::string s, GDlevel &level, int currentBest = 0) {
	return fmt::format(s,
		fmt::arg("name", level.name),
		fmt::arg("best", currentBest),
		fmt::arg("diff", getTextFromKey(getDifficultyName(level))),
		fmt::arg("author", level.author),
		fmt::arg("stars", level.stars));
}

DWORD WINAPI mainThread(LPVOID lpParam)
{
	DRP::InitDiscord();

	// global variable stuff
	updatePresence = false;
	currentPlayerState = playerState::menu;


	// config time!

	// this must be in scope
	std::string saved_level_detail, saved_level_state, saved_level_smalltext,
			playtesting_level_detail, playtesting_level_state, playtesting_level_smalltext,
			error_level_detail, error_level_state, error_level_smalltext,
			editor_detail, editor_state, editor_smalltext,
			menu_detail, menu_state, menu_smalltext,
			user_ranked, user_default;

	// next we fill in defaults to aid in creation
	saved_level_detail = "Playing {name}";
	saved_level_state = "by {author} ({best}%)";
	saved_level_smalltext = "{stars}* {diff}";
	playtesting_level_detail = "Editing a level";
	playtesting_level_state = "";
	playtesting_level_smalltext = "";
	error_level_detail = "Playing a level";
	error_level_state = "";
	error_level_smalltext = "";
	editor_detail = "Editing a level";
	editor_state = "";
	editor_smalltext = "";
	menu_detail = "Idle";
	menu_state = "";
	menu_smalltext = "";
	user_ranked = "{name} [Rank #{rank}]";
	user_default = "";
	bool get_rank = true;

	try {
		const std::string filename = "gdrpc.toml";
		if (!std::ifstream(filename))
		{
			// create generic file
			std::ofstream config_file(filename);

			const toml::value user{
				{"ranked", user_ranked},
				{"default", user_default},
				{"get_rank", get_rank}};

			const toml::value menu{
					{"detail", menu_detail},
					{"state", menu_state},
					{"smalltext", menu_smalltext}};

			const toml::value editor{
					{"detail", editor_detail},
					{"state", editor_state},
					{"smalltext", editor_smalltext}};

			const toml::value saved{
					{"detail", saved_level_detail},
					{"state", saved_level_state},
					{"smalltext", saved_level_smalltext}};

			const toml::value playtesting{
					{"detail", playtesting_level_detail},
					{"state", playtesting_level_state},
					{"smalltext", playtesting_level_smalltext}};

			const toml::value error{
					{"detail", error_level_detail},
					{"state", error_level_state},
					{"smalltext", error_level_smalltext}};

			const toml::value level{
				{"saved", saved},
				{"playtesting", playtesting},
				{"error", error}};

			const toml::value default_config{
				{"level", level},
				{"editor", editor},
				{"menu", menu},
				{"user", user}};

			config_file << "# autogenerated config\n" << default_config << std::endl;

		}
		const toml::value config = toml::parse(filename);

		// setting up strings

		// level
		const auto level_table = toml::find(config, "level");

		// saved
		const auto saved_level_table = toml::find(level_table, "saved");
		saved_level_detail = toml::find_or<std::string>(saved_level_table, "detail", saved_level_detail);
		saved_level_state = toml::find_or<std::string>(saved_level_table, "state", saved_level_state);
		saved_level_smalltext = toml::find_or<std::string>(saved_level_table, "smalltext", saved_level_smalltext);

		// playtesting
		const auto playtesting_level_table = toml::find(level_table, "playtesting");
		playtesting_level_detail = toml::find_or<std::string>(playtesting_level_table, "detail", playtesting_level_detail);
		playtesting_level_state = toml::find_or<std::string>(playtesting_level_table, "state", playtesting_level_state);
		playtesting_level_smalltext = toml::find_or<std::string>(playtesting_level_table, "smalltext", playtesting_level_smalltext);

		// error
		const auto error_level_table = toml::find(level_table, "error");
		error_level_detail = toml::find_or<std::string>(error_level_table, "detail", error_level_detail);
		error_level_state = toml::find_or<std::string>(error_level_table, "state", error_level_state);
		error_level_smalltext = toml::find_or<std::string>(error_level_table, "smalltext", error_level_smalltext);

		// editor
		const auto editor_table = toml::find(config, "editor");
		editor_detail = toml::find_or<std::string>(editor_table, "detail", editor_detail);
		editor_state = toml::find_or<std::string>(editor_table, "state", editor_state);
		editor_smalltext = toml::find_or<std::string>(editor_table, "smalltext", editor_smalltext);

		// menu
		const auto menu_table = toml::find(config, "menu");
		menu_detail = toml::find_or<std::string>(menu_table, "detail", menu_detail);
		menu_state = toml::find_or<std::string>(menu_table, "state", menu_state);
		menu_smalltext = toml::find_or<std::string>(menu_table, "smalltext", menu_smalltext);

		// user (large text)
		const auto user_table = toml::find(config, "user");
		user_ranked = toml::find_or<std::string>(user_table, "ranked", user_ranked);
		user_default = toml::find_or<std::string>(user_table, "default", user_default);
		get_rank = toml::find_or<bool>(user_table, "get_rank", get_rank);
	} catch (const std::exception& e) {
			MessageBoxA(0, e.what(), "config parser error" , MB_OK);
	} catch (...) {
			MessageBoxA(0, "unhandled error\r\nthings should continue fine", "config parser" , MB_OK);
	}

	std::string details, state, smallText, smallImage;
	std::string largeText = user_default;

	// get user
	int *accountID = (int *)(*getBase(0x3222D8) + 0x120);
	GDuser user;
	if (get_rank) {
		bool userInfoSuccess = getUserInfo(*accountID, user);
		if (userInfoSuccess)
		{
			getUserRank(user);
		}
	}

	if (user.rank != -1)
	{
		largeText = fmt::format(
				user_ranked,
				fmt::arg("name", user.name),
				fmt::arg("rank", user.rank));
	}
	else
	{
		char *username = (char *)(*getBase(0x3222D8) + 0x108);
		largeText = std::string(username); // hopeful fallback
	}

	int levelLocation, currentBest;
	GDlevel currentLevel;

	updatePresence = true;

	while (true) {
		// run discord calls
		Discord_RunCallbacks();
		if (updatePresence) {
			switch (currentPlayerState) {
				case playerState::level:
					levelLocation = *(int*)((int)currentGameLevel + 0x364);
					currentBest = *(int *)((int)currentGameLevel + 0x248);
					if (levelLocation == 2) {
						details = playtesting_level_detail;
						state = playtesting_level_state;
						smallText = playtesting_level_smalltext;
						smallImage = "creator_point";
					} else {
						if (!parseGJGameLevel(currentGameLevel, currentLevel)) {
							details = fmt::format(error_level_detail, fmt::arg("best", currentBest));
							state = fmt::format(error_level_state, fmt::arg("best", currentBest));
							smallImage = "";
							smallText = error_level_smalltext;
						}
						else
						{
							details = formatWithLevel(saved_level_detail, currentLevel, currentBest);
							state = formatWithLevel(saved_level_state, currentLevel, currentBest);
							smallText = formatWithLevel(saved_level_smalltext, currentLevel, currentBest);
							smallImage = getDifficultyName(currentLevel);
						}
					}
					break;
				case playerState::editor:
					details = editor_detail;
					state = editor_state;
					smallImage = "creator_point";
					smallText = editor_smalltext;
					break;
				case playerState::menu:
					details = menu_detail;
					state = menu_state;
					smallImage = "";
					smallText = menu_smalltext;
					break;
			}
			updatePresenceS(details, largeText, smallText, state, smallImage);
			updatePresence = false;
		}
		Sleep(1000);
	}

	return 0;
}

