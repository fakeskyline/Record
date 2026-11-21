#include <Geode/Geode.hpp>
#include "modify/Director.hpp"
#include "modify/PauseLayer.hpp"

using namespace geode::prelude;

$on_mod(Loaded) {
    log::info("Record video capture module loaded.");
}
