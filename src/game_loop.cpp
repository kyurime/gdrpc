#include "game_loop.hpp"
#include "pch.h"

Game_Loop game_loop = Game_Loop();

int *get_address(int *start, std::vector<int> addresses) {
  // start with value of 1st address
  auto it = addresses.begin();
  int *cur_addr = (int *)((int)start + *it);
  std::advance(it, 1);

  while (it != addresses.end()) {
    // get next and increment all at once!
    cur_addr = (int *)(*cur_addr + *it);
    std::advance(it, 1); // null pointer protection
  }
  return cur_addr;
}

// insane_demon to Insane Demon
std::string getTextFromKey(std::string key) {
  key[0] = toupper(key[0]); // uppercase first letter
  int index = 1; // start from 1 because we start string from 1, and also
                 // because you can't get an index at -1 like it'll try to do..
  std::for_each(key.begin() + 1, key.end(), [&index, &key](char &letter) {
    // in any case, this checks if it's a space before
    if (key[index - 1] == ' ') {
      letter = toupper(letter); // then capitalizes
    } else if (letter == '_') { // if underscore it goes and remoes it
      letter = ' ';
    }
    index++; // then we use this to count
  });
  return key;
}

std::string formatWithLevel(std::string &s, GDlevel &level,
                            GJGameLevel *in_memory) {
  std::string f;
  try {
    f = fmt::format(
        s, fmt::arg("id", level.levelID), fmt::arg("name", level.name),
        fmt::arg("best", in_memory->current_best),
        fmt::arg("diff", getTextFromKey(getDifficultyName(level))),
        fmt::arg("author", level.author), fmt::arg("stars", level.stars));
  } catch (const fmt::format_error &e) {

    std::string error_string =
        fmt::format("error found while parsing {}\n{}", s, e.what());

    get_game_loop()->get_logger()->critical(error_string);
    MessageBoxA(0, error_string.c_str(), "formatter error", MB_OK);
    f = s;
  } catch (...) {
    get_game_loop()->get_logger()->critical(
        "unknown error found while trying to parse {}", s);
    MessageBoxA(0, "idk", "unknown format error", MB_OK);
    f = s;
  }
  return f;
}

Game_Loop *get_game_loop() { return &game_loop; }

void Game_Loop::update_presence_w(std::string &details, std::string &largeText,
                                  std::string &smallText, std::string &state,
                                  std::string &smallImage) {

  if (logger) {
    logger->info("setting presence:\n\
details: `{}` | state: `{}`\n\
small_text: `{}` | large_text: `{}`\n\
timestamp_update: {}",
                 details, state, smallText, largeText, update_timestamp);
  }

  if (update_timestamp) {
    current_timestamp = time(0);
    update_timestamp = false;
  }
  discord->update(details.c_str(), largeText.c_str(), smallText.c_str(),
                  state.c_str(), smallImage.c_str(), current_timestamp);
}

void Game_Loop::close() {
  if (logger) {
    logger->warn("shutdown called!");
  }
  discord->shutdown();
}

Game_Loop::Game_Loop()
    : player_state(playerState::menu), current_timestamp(time(0)),
      gamelevel(nullptr), update_presence(false), update_timestamp(false),
      discord(get_discord()), logger(nullptr) {
  on_initialize = [] {
  }; // blank function so it doesn't complain about how i don't initialize
}

void Game_Loop::initialize_config() {
  // config time!
  try {
    const std::string filename = "gdrpc.toml";
    if (!std::ifstream(filename)) {
      // create generic file
      std::ofstream config_file(filename);
      config_file << "# autogenerated config\n"
                  << this->config.into_toml() << std::endl;
    }

    const toml::value config = toml::parse(filename);
    this->config.from_toml(config);

  } catch (const std::exception &e) {
    if (logger) {
      logger->critical("error found while parsing config\n{}", e.what());
    }
    MessageBoxA(0, e.what(), "config parser error", MB_OK);
  } catch (...) {
    if (logger) {
      logger->critical("unknown config parsing error");
    }
    MessageBoxA(0, "unhandled error\r\nthings should continue fine",
                "config parser", MB_OK);
  }

  // on debug builds, console logging will be enabled
  if (this->config.settings.logging) {
    logger = spdlog::basic_logger_mt("rpclog", "gdrpc.log", true);
    logger->set_level(spdlog::level::trace);
    logger->flush_on(spdlog::level::debug);
    logger->info("gdrpc v{}", Config::LATEST_VERSION);
  }
}

void Game_Loop::initialize_loop() {
  if (logger) {
    logger->debug("starting setup");
  }

  discord->initialize();

  if (logger) {
    logger->trace("discord initialized");
  }

  large_text = this->config.user.default;

  int *gd_base =
      (int *)GetModuleHandleA(this->config.settings.executable_name.c_str());
  int *accountID = get_address(gd_base, {0x3222D8, 0x120});

  GDuser user;
  if (this->config.user.get_rank) {
    GD_Client client = GD_Client(this->config.settings.base_url,
                                 this->config.settings.url_prefix);
    if (logger) {
      logger->debug("getting infomation for user {}", *accountID);
    }
    try {
      bool userInfoSuccess = client.get_user_info(*accountID, user);
      if (userInfoSuccess) {
        client.get_user_rank(user);
      }
    } catch (const std::exception &e) {
      if (logger) {
        logger->warn("failed to get user info or rank\n{}", e.what());
      }
    }
  }

  if (user.rank != -1) {
    large_text =
        fmt::format(this->config.user.ranked, fmt::arg("name", user.name),
                    fmt::arg("rank", user.rank));
  } else {
    char *username = (char *)(get_address(gd_base, {0x3222D8, 0x108}));
    large_text = std::string(username); // hopeful fallback
  }

  update_presence = true;
  update_timestamp = true;
  on_initialize();
}

void Game_Loop::on_loop() {
  discord->run_callbacks();
  if (update_presence) {
    // we don't need to keep this in the class, every time presence updates it
    // gets remade anyways
    std::string details, state, small_text, small_image;

    switch (player_state) {
    case playerState::level: {
      parseGJGameLevel(gamelevel, level);
      auto level_location = gamelevel->level_type;
      int current_best = gamelevel->current_best;

      if (level_location == GJLevelType::Editor) {
        auto playtesting = this->config.level.playtesting;

        details = formatWithLevel(playtesting.detail, level, this->gamelevel);
        state = formatWithLevel(playtesting.state, level, this->gamelevel);
        small_text =
            formatWithLevel(playtesting.smalltext, level, this->gamelevel);
        small_image = "creator_point";
      } else {
        auto saved = this->config.level.saved;

        details = formatWithLevel(saved.detail, level, this->gamelevel);
        state = formatWithLevel(saved.state, level, this->gamelevel);
        small_text = formatWithLevel(saved.smalltext, level, this->gamelevel);
        small_image = getDifficultyName(level);
      }
      break;
    }
    case playerState::editor: {
      auto editor = this->config.editor;
      parseGJGameLevel(gamelevel, level);

      details = formatWithLevel(editor.detail, level, this->gamelevel);
      state = formatWithLevel(editor.state, level, this->gamelevel);
      small_text = formatWithLevel(editor.smalltext, level, this->gamelevel);
      small_image = "creator_point";
      break;
    }
    case playerState::menu: {
      auto menu = this->config.menu;

      details = menu.detail;
      state = menu.state;
      small_text = menu.smalltext;
      small_image = "";
      break;
    }
    }
    update_presence_w(details, large_text, small_text, state, small_image);
    update_presence = false;
  }
}

void Game_Loop::set_update_presence(bool n_presence) {
  update_presence = n_presence;
}

void Game_Loop::set_update_timestamp(bool n_timestamp) {
  update_timestamp = n_timestamp;
}

void Game_Loop::set_gamelevel(GJGameLevel *n_gamelevel) {
  gamelevel = n_gamelevel;
}

GJGameLevel *Game_Loop::get_gamelevel() { return gamelevel; }

void Game_Loop::set_state(playerState n_state) { player_state = n_state; }

playerState Game_Loop::get_state() { return player_state; }

bool Game_Loop::get_reset_timestamp() {
  return this->config.editor.reset_timestamp;
}

std::string Game_Loop::get_executable_name() {
  return this->config.settings.executable_name;
}

std::shared_ptr<spdlog::logger> Game_Loop::get_logger() { return logger; }

void Game_Loop::register_on_initialize(std::function<void()> callback) {
  on_initialize = callback;
}

DWORD WINAPI mainThread(LPVOID lpParam) {
  game_loop.initialize_loop();
  while (true) {
    try {
    game_loop.on_loop();
    } catch (const std::exception &e) {
      if (auto logger = game_loop.get_logger()) {
        logger->critical("unhandled exception thrown in loop\n{}", e.what());
      }
    }
    Sleep(1000);
  }

  return 0;
}