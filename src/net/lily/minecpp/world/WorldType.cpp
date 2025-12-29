#include "WorldType.hpp"

WorldType* WorldType::worldTypes[16] = { nullptr };

WorldType* WorldType::DEFAULT = (new WorldType(0, "default", 1))->setVersioned();
WorldType* WorldType::FLAT = new WorldType(1, "flat");
WorldType* WorldType::LARGE_BIOMES = new WorldType(2, "largeBiomes");
WorldType* WorldType::AMPLIFIED = (new WorldType(3, "amplified"))->setNotificationData();
WorldType* WorldType::CUSTOMIZED = new WorldType(4, "customized");
WorldType* WorldType::DEBUG_WORLD = new WorldType(5, "debug_all_block_states");
WorldType* WorldType::DEFAULT_1_1 = (new WorldType(8, "default_1_1", 0))->setCanBeCreated(false);