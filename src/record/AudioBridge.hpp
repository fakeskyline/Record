#pragma once
#include <string>
namespace recordaudio_bridge { bool start(std::string const&); void pump(); void stop(); bool available(); }
