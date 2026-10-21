#include "Renderer.hpp"

#include <algorithm>
#include <cmath>
#include <random>

#include <glm/gtc/type_ptr.hpp>

#include "RendererDetail.hpp"
#include "net/lily/minecpp/util/ItemRegistry.hpp"
#include "net/lily/minecpp/Minecraft.hpp"

using RendererDetail::sampleBiomeColor;

namespace {
const char* blockParticleTexture(const BlockAtlas& atlas, const Block block) {
    const auto named = [&](const std::string& name) { return BlockUtil::getAtlasTexturePtr(atlas, name); };
    switch (blockId(block)) {
        case blockId(BLOCK_GRASS): return named("dirt");
        case blockId(BLOCK_BED): return named((blockMeta(block) & 8) != 0 ? "bed_head_top" : "bed_feet_top");
        case blockId(BLOCK_END_PORTAL_FRAME): return named("endframe_side");
        case blockId(BLOCK_SLIME): return named("slime");
        case blockId(BLOCK_IRON_BARS): return named("iron_bars");
        case blockId(BLOCK_GLASS_PANE): return named("glass");
        case blockId(BLOCK_STAINED_GLASS_PANE): return named(std::string("glass_")
            + BlockUtil::dyeColorNames[blockMeta(block) & 15]);
        case blockId(BLOCK_STONE_BUTTON): case blockId(BLOCK_STONE_PRESSURE_PLATE): return named("stone");
        case blockId(BLOCK_WOODEN_BUTTON): case blockId(BLOCK_WOODEN_PRESSURE_PLATE):
        case blockId(BLOCK_STANDING_SIGN): case blockId(BLOCK_WALL_SIGN): return named("planks_oak");
        case blockId(BLOCK_LIGHT_WEIGHTED_PRESSURE_PLATE): return named("gold_block");
        case blockId(BLOCK_HEAVY_WEIGHTED_PRESSURE_PLATE): return named("iron_block");
        case blockId(BLOCK_ENCHANTING_TABLE): return named("enchanting_table_side");
        case blockId(BLOCK_FLOWER_POT): return named("flower_pot");
        case blockId(BLOCK_CAULDRON): return named("cauldron_side");
        case blockId(BLOCK_SOUL_SAND): return named("soul_sand");
        case blockId(BLOCK_VINE): return named("vine");
        case blockId(BLOCK_REDSTONE_WIRE): return named("redstone_dust_cross");
        case blockId(BLOCK_UNPOWERED_REPEATER): return named("repeater_off");
        case blockId(BLOCK_POWERED_REPEATER): return named("repeater_on");
        case blockId(BLOCK_UNPOWERED_COMPARATOR): return named("comparator_off");
        case blockId(BLOCK_POWERED_COMPARATOR): return named("comparator_on");
        case blockId(BLOCK_CARPET): return named(std::string("wool_colored_")
            + BlockUtil::dyeColorNames[blockMeta(block) & 15]);
        case blockId(BLOCK_FENCE): case blockId(BLOCK_FENCE_GATE): return named("planks_oak");
        case blockId(BLOCK_SPRUCE_FENCE): case blockId(BLOCK_SPRUCE_FENCE_GATE): return named("planks_spruce");
        case blockId(BLOCK_BIRCH_FENCE): case blockId(BLOCK_BIRCH_FENCE_GATE): return named("planks_birch");
        case blockId(BLOCK_JUNGLE_FENCE): case blockId(BLOCK_JUNGLE_FENCE_GATE): return named("planks_jungle");
        case blockId(BLOCK_DARK_OAK_FENCE): case blockId(BLOCK_DARK_OAK_FENCE_GATE): return named("planks_big_oak");
        case blockId(BLOCK_ACACIA_FENCE): case blockId(BLOCK_ACACIA_FENCE_GATE): return named("planks_acacia");
        case blockId(BLOCK_NETHER_BRICK_FENCE): return named("nether_brick");
        case blockId(BLOCK_COBBLESTONE_WALL):
            return named((blockMeta(block) & 1) != 0 ? "cobblestone_mossy" : "cobblestone");
        default: return BlockUtil::getBlockTexture(block, 4);
    }
}
}

void Renderer::addBlockParticle(const glm::vec3 position, const glm::vec3 velocity,
                                const Block block, const float scale,
                                const float velocityMultiplier) {
    if (blockId(block) == blockId(BLOCK_AIR) || particles.size() >= 4096) return;

    thread_local std::mt19937 random(std::random_device{}());
    std::uniform_real_distribution unit(0.0f, 1.0f);
    std::uniform_real_distribution centered(-0.4f, 0.4f);
    const char* texture = blockParticleTexture(blockAtlas.second, block);
    const auto [tileX, tileY] = blockAtlas.second.blockAtlasPos.at(texture);
    Particle particle;
    particle.previousPosition = position;
    particle.position = position;
    glm::vec3 randomizedVelocity = velocity + glm::vec3(
        centered(random), centered(random), centered(random));
    const float speed = glm::length(randomizedVelocity);
    const float motionScale = (unit(random) + unit(random) + 1.0f) * 0.15f;
    if (speed > 1.0E-6f) randomizedVelocity *= motionScale * 0.4f / speed;
    randomizedVelocity.y += 0.1f;
    randomizedVelocity.x *= velocityMultiplier;
    randomizedVelocity.y = (randomizedVelocity.y - 0.1f) * velocityMultiplier + 0.1f;
    randomizedVelocity.z *= velocityMultiplier;
    particle.velocity = randomizedVelocity;
    particle.size = (unit(random) * 0.5f + 0.5f) * 0.1f * scale;
    particle.collisionRadius = particle.size * 0.9f;
    particle.maximumAge = static_cast<int>(4.0f / (unit(random) * 0.9f + 0.1f));
    particle.tile = static_cast<uint16_t>(tileY * atlasTilesPerRow + tileX);
    particle.textureX = unit(random) * 3.0f;
    particle.textureY = unit(random) * 3.0f;
    const uint8_t biome = mc->world->getBiomeAt(static_cast<int>(std::floor(position.x)),
        static_cast<int>(std::floor(position.z)));
    switch (blockId(block)) {
        case blockId(BLOCK_TALLGRASS):
        case blockId(BLOCK_DOUBLE_PLANT):
            particle.color = sampleBiomeColor(biome, false);
            break;
        case blockId(BLOCK_LEAVES):
            switch (blockMeta(block) & 3) {
                case 1: particle.color = {0.3804f, 0.6000f, 0.3804f}; break;
                case 2: particle.color = {0.5020f, 0.6549f, 0.3333f}; break;
                default: particle.color = sampleBiomeColor(biome, true); break;
            }
            break;
        case blockId(BLOCK_LEAVES2):
        case blockId(BLOCK_VINE):
            particle.color = sampleBiomeColor(biome, true);
            break;
        case blockId(BLOCK_WATERLILY):
            particle.color = sampleBiomeColor(biome, false);
            break;
        default:
            break;
    }
    particles.push_back(particle);
    mc->activeParticleCount.store(particles.size(), std::memory_order_relaxed);
}

void Renderer::addBlockHitParticle(const glm::ivec3& position, const uint8_t facing,
                                   const Block block) {
    if (mc->settings->getInt("particles") == 2) return;
    thread_local std::mt19937 random(std::random_device{}());
    std::uniform_real_distribution offset(0.1f, 0.9f);
    glm::vec3 particlePosition = glm::vec3(position) + glm::vec3(offset(random), offset(random), offset(random));
    constexpr float edgeOffset = 0.1f;
    switch (facing) {
        case 0: particlePosition.y = position.y - edgeOffset; break;
        case 1: particlePosition.y = position.y + 1.0f + edgeOffset; break;
        case 2: particlePosition.z = position.z - edgeOffset; break;
        case 3: particlePosition.z = position.z + 1.0f + edgeOffset; break;
        case 4: particlePosition.x = position.x - edgeOffset; break;
        case 5: particlePosition.x = position.x + 1.0f + edgeOffset; break;
        default: break;
    }
    addBlockParticle(particlePosition, glm::vec3(0.0f), block, 0.6f, 0.2f);
}

void Renderer::addBlockDestroyParticles(const glm::ivec3& position, const Block block) {
    const int setting = mc->settings->getInt("particles");
    const int divisions = setting == 2 ? 2 : setting == 1 ? 3 : 4;
    for (int x = 0; x < divisions; ++x) {
        for (int y = 0; y < divisions; ++y) {
            for (int z = 0; z < divisions; ++z) {
                const glm::vec3 local(
                    (static_cast<float>(x) + 0.5f) / divisions,
                    (static_cast<float>(y) + 0.5f) / divisions,
                    (static_cast<float>(z) + 0.5f) / divisions);
                addBlockParticle(glm::vec3(position) + local, local - glm::vec3(0.5f), block);
            }
        }
    }
}

void Renderer::addBlockRunningParticle(const glm::dvec3 &position, const glm::dvec3 &velocity,
                                       const float entityWidth, const Block block) {
    if (blockId(block) == blockId(BLOCK_AIR)) return;
    thread_local std::mt19937 random(std::random_device{}());
    std::uniform_real_distribution offset(-0.5f, 0.5f);
    const glm::vec3 particlePosition(
        position.x + offset(random) * entityWidth,
        position.y + 0.1,
        position.z + offset(random) * entityWidth);
    addBlockParticle(particlePosition,
        glm::vec3(-velocity.x * 4.0, 1.5, -velocity.z * 4.0), block);
}

void Renderer::addBlockLandingParticles(const glm::dvec3 &position, const float entityWidth,
                                        const Block block, int count) {
    if (mc->settings->getInt("particles") == 2) count /= 3;
    else if (mc->settings->getInt("particles") == 1) count /= 2;
    thread_local std::mt19937 random(std::random_device{}());
    std::uniform_real_distribution offset(-0.5f, 0.5f);
    for (int particle = 0; particle < count; ++particle) {
        const glm::vec3 particlePosition(position.x + offset(random) * entityWidth,
            position.y + 0.1, position.z + offset(random) * entityWidth);
        addBlockParticle(particlePosition, {0.0f, 0.15f, 0.0f}, block, 1.0f, 0.15f);
    }
}

namespace {
enum ParticleKind : uint8_t {
    KIND_BLOCK = 0, KIND_GENERIC, KIND_FLAME, KIND_SMOKE, KIND_EXPLODE, KIND_LARGE_EXPLODE, KIND_HUGE_EXPLODE,
    KIND_BUBBLE, KIND_RAIN, KIND_WAKE, KIND_SUSPEND, KIND_AURA, KIND_CRIT, KIND_SPELL, KIND_DRIP, KIND_HEART,
    KIND_NOTE, KIND_PORTAL, KIND_ENCHANT, KIND_LAVA, KIND_CLOUD, KIND_REDDUST, KIND_FOOTSTEP, KIND_BREAKING,
    KIND_FIREWORK, KIND_SNOW_SHOVEL
};

std::mt19937& generator() {
    thread_local std::mt19937 random(std::random_device{}());
    return random;
}
float uniform() { return std::uniform_real_distribution<float>(0.0f, 1.0f)(generator()); }
double signedUnit() { return static_cast<double>(uniform()) * 2.0 - 1.0; } // math.random() * 2 - 1
bool isWater(const Block block) {
    return blockId(block) == blockId(BLOCK_WATER) || blockId(block) == blockId(BLOCK_FLOWING_WATER);
}
}

// entityFX's base constructors
Renderer::Particle Renderer::makeParticle(const uint8_t kind, const glm::dvec3& position) {
    Particle particle;
    particle.kind = kind;
    particle.sheet = 1;
    particle.previousPosition = particle.position = position;
    particle.collisionRadius = 0.1f;
    particle.gravity = 0.0f;
    particle.colorScale = 1.0f;
    particle.baseSize = (uniform() * 0.5f + 0.5f) * 2.0f * 0.1f;
    particle.size = particle.baseSize;
    particle.maximumAge = static_cast<int>(4.0f / (uniform() * 0.9f + 0.1f));
    particle.textureX = uniform() * 3.0f;
    particle.textureY = uniform() * 3.0f;
    return particle;
}

static glm::dvec3 baseVelocity(const glm::dvec3& given) {
    glm::dvec3 motion = given + glm::dvec3(signedUnit(), signedUnit(), signedUnit()) * 0.4;
    const double scale = (static_cast<double>(uniform()) + uniform() + 1.0) * 0.15;
    const double length = std::max(glm::length(motion), 1.0E-9);
    motion = motion / length * scale * 0.4;
    motion.y += 0.1;
    return motion;
}

void Renderer::queueServerParticle(const int type, const glm::dvec3& position, const glm::dvec3& offset,
                                   const float speed, const int count, const int* arguments) {
    PendingParticle pending;
    pending.type = type;
    pending.position = position;
    pending.offset = offset;
    pending.speed = speed;
    pending.count = count;
    pending.arguments[0] = arguments != nullptr ? arguments[0] : 0;
    pending.arguments[1] = arguments != nullptr ? arguments[1] : 0;
    std::lock_guard lock(pendingParticleMutex);
    if (pendingParticles.size() < 8192) pendingParticles.push_back(pending);
}

void Renderer::createParticle(const int type, const glm::dvec3& position, const glm::dvec3& given,
                              const int* arguments) {
    if (particles.size() >= 4096) return;
    using glm::dvec3;
    const auto add = [&](Particle particle) { particles.push_back(particle); };
    switch (type) {
        case 0: { // entityExplodeFX
            Particle p = makeParticle(KIND_EXPLODE, position);
            p.velocity = given + dvec3(signedUnit(), signedUnit(), signedUnit()) * 0.05;
            p.color = glm::vec3(uniform() * 0.3f + 0.7f);
            p.baseSize = (uniform() * uniform() * 6.0f + 1.0f) * 0.1f;
            p.size = p.baseSize;
            p.maximumAge = static_cast<int>(16.0 / (uniform() * 0.8 + 0.2)) + 2;
            add(p);
            break;
        }
        case 1: { // entityLargeExplodeFX: a 16 frame sprite sheet
            Particle p = makeParticle(KIND_LARGE_EXPLODE, position);
            p.sheet = 4;
            p.noClip = true;
            p.maximumAge = 6 + static_cast<int>(uniform() * 4.0f);
            p.color = glm::vec3(uniform() * 0.6f + 0.4f);
            p.size = 2.0f * (1.0f - static_cast<float>(given.x) * 0.5f);
            add(p);
            break;
        }
        case 2: { // entityHugeExplodeFX: spawns large explosions for 8 ticks
            Particle p = makeParticle(KIND_HUGE_EXPLODE, position);
            p.noClip = true;
            p.maximumAge = 8;
            add(p);
            break;
        }
        case 3: { // firework spark
            Particle p = makeParticle(KIND_FIREWORK, position);
            p.velocity = given;
            p.baseSize *= 0.75f;
            p.size = p.baseSize;
            p.maximumAge = 48 + static_cast<int>(uniform() * 12.0f);
            p.sprite = 160;
            add(p);
            break;
        }
        case 4: { // entityBubbleFX
            Particle p = makeParticle(KIND_BUBBLE, position);
            p.collisionRadius = 0.01f;
            p.sprite = 32;
            p.baseSize *= uniform() * 0.6f + 0.2f;
            p.size = p.baseSize;
            p.velocity = given * 0.2 + dvec3(signedUnit(), signedUnit(), signedUnit()) * 0.02;
            p.maximumAge = static_cast<int>(8.0 / (uniform() * 0.8 + 0.2));
            add(p);
            break;
        }
        case 5: case 39: { // entitySplashFX / EntityRainFX (the splash keeps a little gravity)
            Particle p = makeParticle(KIND_RAIN, position);
            p.velocity = baseVelocity({0.0, 0.0, 0.0});
            p.velocity.x *= 0.3;
            p.velocity.y = static_cast<double>(uniform()) * 0.2 + 0.1;
            p.velocity.z *= 0.3;
            p.collisionRadius = 0.005f;
            p.sprite = 19 + static_cast<int>(uniform() * 4.0f);
            p.maximumAge = static_cast<int>(8.0 / (uniform() * 0.8 + 0.2));
            p.gravity = 0.06f;
            if (type == 5) {
                p.gravity = 0.04f;
                p.sprite += 1;
                if (given.y == 0.0 && (given.x != 0.0 || given.z != 0.0))
                    p.velocity = {given.x, given.y + 0.1, given.z};
            }
            add(p);
            break;
        }
        case 6: { // entityFishWakeFX
            Particle p = makeParticle(KIND_WAKE, position);
            p.collisionRadius = 0.005f;
            p.sprite = 19;
            p.maximumAge = static_cast<int>(8.0 / (uniform() * 0.8 + 0.2));
            p.velocity = given;
            add(p);
            break;
        }
        case 7: { // entitySuspendFX
            Particle p = makeParticle(KIND_SUSPEND, position - dvec3(0.0, 0.125, 0.0));
            p.color = {0.4f, 0.4f, 0.7f};
            p.sprite = 0;
            p.collisionRadius = 0.005f;
            p.baseSize *= uniform() * 0.6f + 0.2f;
            p.size = p.baseSize;
            p.maximumAge = static_cast<int>(16.0 / (uniform() * 0.8 + 0.2));
            add(p);
            break;
        }
        case 8: case 21: case 22: { // entityAuraFX: depth suspend, happy villager, town aura
            Particle p = makeParticle(KIND_AURA, position);
            p.velocity = baseVelocity(given) * 0.02;
            p.color = glm::vec3(uniform() * 0.1f + 0.2f);
            p.collisionRadius = 0.01f;
            p.baseSize *= uniform() * 0.6f + 0.5f;
            p.size = p.baseSize;
            p.maximumAge = static_cast<int>(20.0 / (uniform() * 0.8 + 0.2));
            p.noClip = true;
            p.sprite = 0;
            if (type == 21) { p.sprite = 82; p.color = glm::vec3(1.0f); }
            add(p);
            break;
        }
        case 9: case 10: { // entityCrit2FX
            Particle p = makeParticle(KIND_CRIT, position);
            p.velocity = baseVelocity({0.0, 0.0, 0.0}) * 0.1 + given * 0.4;
            p.color = glm::vec3(static_cast<float>(uniform() * 0.3 + 0.6));
            p.baseSize *= 0.75f;
            p.size = p.baseSize;
            p.maximumAge = static_cast<int>(6.0 / (uniform() * 0.8 + 0.6));
            p.sprite = 65;
            if (type == 10) {
                p.color = {p.color.r * 0.3f, p.color.g * 0.8f, p.color.b};
                p.sprite = 66;
            }
            add(p);
            updateParticle(particles.back(), mc->world);
            break;
        }
        case 11: case 12: { // entitySmokeFX, and the large smoke (EntityCritFX with scale 2.5)
            const float scale = type == 12 ? 2.5f : 1.0f;
            Particle p = makeParticle(KIND_SMOKE, position);
            p.velocity = baseVelocity({0.0, 0.0, 0.0}) * 0.0;
            p.velocity = (baseVelocity({0.0, 0.0, 0.0}) * 0.1) + given;
            p.color = glm::vec3(static_cast<float>(uniform() * 0.3));
            p.baseSize *= 0.75f * scale;
            p.size = p.baseSize;
            p.maximumAge = static_cast<int>(static_cast<float>(static_cast<int>(8.0 / (uniform() * 0.8 + 0.2))) * scale);
            add(p);
            break;
        }
        case 13: case 14: case 15: case 16: case 17: { // entitySpellParticleFX
            Particle p = makeParticle(KIND_SPELL, position);
            p.velocity = baseVelocity({0.5 - uniform(), given.y, 0.5 - uniform()});
            p.velocity.y *= 0.2;
            if (given.x == 0.0 && given.z == 0.0) { p.velocity.x *= 0.1; p.velocity.z *= 0.1; }
            p.baseSize *= 0.75f;
            p.size = p.baseSize;
            p.maximumAge = static_cast<int>(8.0 / (uniform() * 0.8 + 0.2));
            p.extra = 128.0f;
            if (type == 14 || type == 17) p.extra = 144.0f;
            if (type == 15 || type == 16) p.color = {static_cast<float>(given.x), static_cast<float>(given.y), static_cast<float>(given.z)};
            if (type == 16) p.alpha = 0.15f;
            if (type == 17) {
                const float shade = uniform() * 0.5f + 0.35f;
                p.color = {shade, 0.0f, shade};
            }
            p.sprite = static_cast<int>(p.extra);
            add(p);
            break;
        }
        case 18: case 19: { // entityDropParticleFX
            Particle p = makeParticle(KIND_DRIP, position);
            p.collisionRadius = 0.005f;
            p.color = type == 18 ? glm::vec3(0.0f, 0.0f, 1.0f) : glm::vec3(1.0f, 0.0f, 0.0f);
            p.sprite = 113;
            p.gravity = 0.06f;
            p.extra = 40.0f; // hangs for 40 ticks before falling
            p.maximumAge = static_cast<int>(64.0 / (uniform() * 0.8 + 0.2));
            p.anchor = {static_cast<double>(type), 0.0, 0.0}; // 18 water, 19 lava
            add(p);
            break;
        }
        case 20: case 34: { // entityHeartFX, angry villager
            Particle p = makeParticle(KIND_HEART, position);
            p.velocity = baseVelocity({0.0, 0.0, 0.0}) * 0.01;
            p.velocity.y += 0.1;
            p.baseSize *= 0.75f * 2.0f;
            p.size = p.baseSize;
            p.maximumAge = 16;
            p.sprite = type == 20 ? 81 : 80;
            if (type == 20) p.color = glm::vec3(1.0f);
            add(p);
            break;
        }
        case 23: { // entityNoteFX: the color is chosen by the note
            Particle p = makeParticle(KIND_NOTE, position);
            p.velocity = baseVelocity({0.0, 0.0, 0.0}) * 0.01;
            p.velocity.y += 0.2;
            constexpr float tau = 6.2831853f;
            const float note = static_cast<float>(given.x);
            p.color = {std::sin(note * tau) * 0.65f + 0.35f, std::sin((note + 0.33333334f) * tau) * 0.65f + 0.35f,
                       std::sin((note + 0.6666667f) * tau) * 0.65f + 0.35f};
            p.baseSize *= 0.75f * 2.0f;
            p.size = p.baseSize;
            p.maximumAge = 6;
            p.sprite = 64;
            add(p);
            break;
        }
        case 24: { // entityPortalFX
            Particle p = makeParticle(KIND_PORTAL, position);
            p.velocity = given;
            p.anchor = position;
            const float shade = uniform() * 0.6f + 0.4f;
            p.baseSize = (uniform() * 0.2f + 0.5f) * 0.1f;
            p.size = p.baseSize;
            p.color = {shade * 0.9f, shade * 0.3f, shade};
            p.maximumAge = static_cast<int>(uniform() * 10.0f) + 40;
            p.noClip = true;
            p.sprite = static_cast<int>(uniform() * 8.0f);
            add(p);
            break;
        }
        case 25: { // entityEnchantmentTableParticleFX
            Particle p = makeParticle(KIND_ENCHANT, position);
            p.velocity = given;
            p.anchor = position;
            const float shade = uniform() * 0.6f + 0.4f;
            p.baseSize = (uniform() * 0.5f + 0.2f) * 0.1f;
            p.size = p.baseSize;
            p.color = {shade * 0.9f, shade * 0.9f, shade};
            p.maximumAge = static_cast<int>(uniform() * 10.0f) + 30;
            p.noClip = true;
            p.sprite = static_cast<int>(uniform() * 26.0f + 1.0f + 224.0f);
            add(p);
            break;
        }
        case 26: { // entityFlameFX
            Particle p = makeParticle(KIND_FLAME, position + dvec3(uniform() - uniform(), uniform() - uniform(),
                uniform() - uniform()) * 0.05);
            p.velocity = baseVelocity({0.0, 0.0, 0.0}) * 0.01 + given;
            p.maximumAge = static_cast<int>(8.0 / (uniform() * 0.8 + 0.2)) + 4;
            p.noClip = true;
            p.sprite = 48;
            add(p);
            break;
        }
        case 27: { // entityLavaFX
            Particle p = makeParticle(KIND_LAVA, position);
            p.velocity = baseVelocity({0.0, 0.0, 0.0}) * 0.8;
            p.velocity.y = uniform() * 0.4f + 0.05f;
            p.baseSize *= uniform() * 2.0f + 0.2f;
            p.size = p.baseSize;
            p.maximumAge = static_cast<int>(16.0 / (uniform() * 0.8 + 0.2));
            p.sprite = 49;
            add(p);
            break;
        }
        case 28: { // entityFootStepFX
            Particle p = makeParticle(KIND_FOOTSTEP, position);
            p.sheet = 3;
            p.noClip = true;
            p.maximumAge = 200;
            p.baseSize = 0.125f;
            p.size = 0.125f;
            add(p);
            break;
        }
        case 29: { // entityCloudFX
            Particle p = makeParticle(KIND_CLOUD, position);
            p.velocity = baseVelocity({0.0, 0.0, 0.0}) * 0.1 + given;
            p.color = glm::vec3(1.0f - static_cast<float>(uniform() * 0.3));
            p.baseSize *= 0.75f * 2.5f;
            p.size = p.baseSize;
            p.maximumAge = static_cast<int>(static_cast<float>(static_cast<int>(8.0 / (uniform() * 0.8 + 0.3))) * 2.5f);
            add(p);
            break;
        }
        case 30: { // entityReddustFX: the speeds are the color
            Particle p = makeParticle(KIND_REDDUST, position);
            p.velocity = baseVelocity({0.0, 0.0, 0.0}) * 0.1;
            const float red = given.x == 0.0 ? 1.0f : static_cast<float>(given.x);
            const float brightness = uniform() * 0.4f + 0.6f;
            p.color = {(static_cast<float>(uniform() * 0.2) + 0.8f) * red * brightness,
                       (static_cast<float>(uniform() * 0.2) + 0.8f) * static_cast<float>(given.y) * brightness,
                       (static_cast<float>(uniform() * 0.2) + 0.8f) * static_cast<float>(given.z) * brightness};
            p.baseSize *= 0.75f;
            p.size = p.baseSize;
            p.maximumAge = static_cast<int>(8.0 / (uniform() * 0.8 + 0.2));
            add(p);
            break;
        }
        case 31: case 33: case 36: { // entityBreakingFX: snowball, slime ball, any item
            int item = type == 31 ? 332 : type == 33 ? 341 : (arguments != nullptr ? arguments[0] : 0);
            const int damage = type == 36 && arguments != nullptr ? arguments[1] : 0;
            Particle p = makeParticle(KIND_BREAKING, position);
            p.sheet = 2;
            p.tile = itemAtlas.tile(ItemRegistry::identifier(static_cast<int16_t>(item), static_cast<int16_t>(damage)));
            p.velocity = baseVelocity({0.0, 0.0, 0.0}) * 0.1 + (type == 36 ? given : dvec3(0.0));
            p.gravity = 1.0f;
            p.baseSize /= 2.0f;
            p.size = p.baseSize;
            add(p);
            break;
        }
        case 32: { // entitySnowShovelFX
            Particle p = makeParticle(KIND_SNOW_SHOVEL, position);
            p.velocity = baseVelocity({0.0, 0.0, 0.0}) * 0.1 + given;
            p.color = glm::vec3(1.0f - static_cast<float>(uniform() * 0.3));
            p.baseSize *= 0.75f;
            p.size = p.baseSize;
            p.maximumAge = static_cast<int>(8.0 / (uniform() * 0.8 + 0.2));
            add(p);
            break;
        }
        case 37: case 38: { // block crack and block dust: block fragments with the given motion
            const Block block = static_cast<Block>(((arguments != nullptr ? arguments[0] : 0) & 0xFFF) << 4
                | ((arguments != nullptr ? arguments[0] : 0) >> 12 & 15));
            addBlockParticle(glm::vec3(position), glm::vec3(0.0f), block);
            if (!particles.empty() && particles.back().kind == KIND_BLOCK) particles.back().velocity = given;
            break;
        }
        default: break; // barrier, item take, mob appearance
    }
}

void Renderer::updateParticle(Particle& particle, const World* world) {
    // entity.moveEntity for a particle: slide against the blocks, stop on a hit
    bool onGround = false;
    bool blockedHorizontally = false;
    const auto move = [&](double dx, double dy, double dz) {
        const double requestedX = dx, requestedY = dy, requestedZ = dz;
        if (!particle.noClip && world != nullptr) {
            const double radius = particle.collisionRadius;
            AABB bounds(particle.position.x - radius, particle.position.y - radius,
                particle.position.z - radius, particle.position.x + radius,
                particle.position.y + radius, particle.position.z + radius);
            const std::vector<LocalBlock> collisions = world->getCollidingBlocks(bounds.expand(dx, dy, dz));
            for (const LocalBlock& collision : collisions) dy = collision.calculateYOffset(bounds, dy);
            bounds = bounds.offset(0.0, dy, 0.0);
            for (const LocalBlock& collision : collisions) dx = collision.calculateXOffset(bounds, dx);
            bounds = bounds.offset(dx, 0.0, 0.0);
            for (const LocalBlock& collision : collisions) dz = collision.calculateZOffset(bounds, dz);
        }
        particle.position += glm::dvec3(dx, dy, dz);
        onGround = requestedY != dy && requestedY < 0.0;
        blockedHorizontally = requestedX != dx || requestedZ != dz;
        if (requestedX != dx) particle.velocity.x = 0.0;
        if (requestedY != dy) particle.velocity.y = 0.0;
        if (requestedZ != dz) particle.velocity.z = 0.0;
    };
    const auto blockAtParticle = [&]() {
        return world != nullptr ? world->getBlockAt(static_cast<int>(std::floor(particle.position.x)),
            static_cast<int>(std::floor(particle.position.y)), static_cast<int>(std::floor(particle.position.z)))
            : BLOCK_AIR;
    };
    const auto groundFriction = [&] {
        if (onGround) { particle.velocity.x *= 0.7; particle.velocity.z *= 0.7; }
    };
    const auto frameSprite = [&] { return 7 - particle.age * 8 / std::max(particle.maximumAge, 1); };
    const double previousY = particle.position.y;
    glm::dvec3& velocity = particle.velocity;
    ++particle.age;

    switch (particle.kind) {
        case KIND_BLOCK: case KIND_GENERIC: case KIND_BREAKING: case KIND_SNOW_SHOVEL:
            if (particle.kind == KIND_SNOW_SHOVEL) {
                particle.sprite = frameSprite();
                velocity.y -= 0.03;
                move(velocity.x, velocity.y, velocity.z);
                velocity *= 0.99;
                groundFriction();
            } else {
                velocity.y -= 0.04 * particle.gravity;
                move(velocity.x, velocity.y, velocity.z);
                velocity *= 0.98;
                groundFriction();
            }
            break;
        case KIND_FLAME:
            move(velocity.x, velocity.y, velocity.z);
            velocity *= 0.96;
            groundFriction();
            break;
        case KIND_SMOKE: case KIND_CLOUD: case KIND_REDDUST: case KIND_EXPLODE: case KIND_SPELL: {
            if (particle.kind == KIND_SPELL) particle.sprite = static_cast<int>(particle.extra) + frameSprite();
            else particle.sprite = frameSprite();
            if (particle.kind != KIND_CLOUD && particle.kind != KIND_REDDUST) velocity.y += 0.004;
            move(velocity.x, velocity.y, velocity.z);
            if (particle.kind != KIND_EXPLODE && particle.kind != KIND_CLOUD && particle.position.y == previousY) {
                velocity.x *= 1.1;
                velocity.z *= 1.1;
            }
            velocity *= particle.kind == KIND_EXPLODE ? 0.9 : 0.96;
            groundFriction();
            if (particle.kind == KIND_CLOUD && mc != nullptr && mc->player != nullptr
                && glm::distance(particle.position, glm::dvec3(mc->player->position)) < 2.0
                && particle.position.y > mc->player->getBoundingBox().minY) {
                particle.position.y += (mc->player->getBoundingBox().minY - particle.position.y) * 0.2;
                velocity.y += (mc->player->velocity.y - velocity.y) * 0.2;
            }
            break;
        }
        case KIND_LARGE_EXPLODE: break;
        case KIND_HUGE_EXPLODE: {
            for (int index = 0; index < 6; ++index) {
                const glm::dvec3 offset((uniform() - uniform()) * 4.0, (uniform() - uniform()) * 4.0,
                    (uniform() - uniform()) * 4.0);
                createParticle(1, particle.position + offset,
                    {static_cast<double>(particle.age - 1) / 8.0, 0.0, 0.0}, nullptr);
            }
            break;
        }
        case KIND_BUBBLE:
            velocity.y += 0.002;
            move(velocity.x, velocity.y, velocity.z);
            velocity *= 0.85;
            if (!isWater(blockAtParticle())) particle.age = particle.maximumAge;
            break;
        case KIND_RAIN: {
            velocity.y -= particle.gravity;
            move(velocity.x, velocity.y, velocity.z);
            velocity *= 0.98;
            if (onGround) {
                if (uniform() < 0.5f) particle.age = particle.maximumAge;
                velocity.x *= 0.7;
                velocity.z *= 0.7;
            }
            const Block block = blockAtParticle();
            if (isWater(block) || BlockUtil::isFullBlock(block)) {
                const double top = std::floor(particle.position.y) + (isWater(block) ? 0.89 : 1.0);
                if (particle.position.y < top) particle.age = particle.maximumAge;
            }
            break;
        }
        case KIND_WAKE: {
            const int step = 60 - (particle.maximumAge - particle.age);
            move(velocity.x, velocity.y, velocity.z);
            velocity *= 0.98;
            particle.sprite = 19 + std::max(step, 0) % 4;
            break;
        }
        case KIND_SUSPEND:
            move(velocity.x, velocity.y, velocity.z);
            if (!isWater(blockAtParticle())) particle.age = particle.maximumAge;
            break;
        case KIND_AURA:
            move(velocity.x, velocity.y, velocity.z);
            velocity *= 0.99;
            break;
        case KIND_CRIT:
            move(velocity.x, velocity.y, velocity.z);
            particle.color.g *= 0.96f;
            particle.color.b *= 0.9f;
            velocity *= 0.7;
            velocity.y -= 0.02;
            groundFriction();
            break;
        case KIND_DRIP: {
            const bool lava = particle.anchor.x == 19.0;
            if (lava) {
                const float phase = 40.0f - particle.extra;
                particle.color = {1.0f, 16.0f / (phase + 16.0f), 4.0f / (phase + 8.0f)};
            }
            velocity.y -= particle.gravity;
            if (particle.extra-- > 0.0f) {
                velocity *= 0.02;
                particle.sprite = 113;
            } else {
                particle.sprite = 112;
            }
            move(velocity.x, velocity.y, velocity.z);
            velocity *= 0.98;
            if (particle.age >= particle.maximumAge) break;
            if (onGround) {
                if (lava) {
                    particle.age = particle.maximumAge;
                    createParticle(11, particle.position, {0.0, 0.0, 0.0}, nullptr);
                } else {
                    particle.sprite = 114;
                }
                velocity.x *= 0.7;
                velocity.z *= 0.7;
            }
            break;
        }
        case KIND_HEART:
            move(velocity.x, velocity.y, velocity.z);
            if (particle.position.y == previousY) { velocity.x *= 1.1; velocity.z *= 1.1; }
            velocity *= 0.86;
            groundFriction();
            break;
        case KIND_NOTE:
            move(velocity.x, velocity.y, velocity.z);
            if (particle.position.y == previousY) { velocity.x *= 1.1; velocity.z *= 1.1; }
            velocity *= 0.66;
            groundFriction();
            break;
        case KIND_PORTAL: {
            float f = static_cast<float>(particle.age - 1) / static_cast<float>(particle.maximumAge);
            f = 1.0f - (-f + f * f * 2.0f);
            particle.position = particle.anchor + velocity * static_cast<double>(f);
            particle.position.y += static_cast<double>(1.0f - f);
            break;
        }
        case KIND_ENCHANT: {
            const float f = 1.0f - static_cast<float>(particle.age - 1) / static_cast<float>(particle.maximumAge);
            float f1 = 1.0f - f;
            f1 = f1 * f1;
            f1 = f1 * f1;
            particle.position = particle.anchor + velocity * static_cast<double>(f);
            particle.position.y -= static_cast<double>(f1 * 1.2f);
            break;
        }
        case KIND_LAVA: {
            const float f = static_cast<float>(particle.age) / static_cast<float>(particle.maximumAge);
            if (uniform() > f) createParticle(11, particle.position, velocity, nullptr);
            velocity.y -= 0.03;
            move(velocity.x, velocity.y, velocity.z);
            velocity *= 0.999;
            groundFriction();
            break;
        }
        case KIND_FOOTSTEP: break;
        case KIND_FIREWORK:
            particle.sprite = 160 + std::max(frameSprite(), 0);
            velocity.y -= 0.004;
            move(velocity.x, velocity.y, velocity.z);
            velocity *= 0.91;
            groundFriction();
            break;
        default: break;
    }
    (void)blockedHorizontally;
}

void Renderer::emitParticlesAtEntity(const int entityId, const int particleType) {
    std::lock_guard lock(emitterMutex);
    emitters.push_back({entityId, particleType, 0});
}

// block.randomDisplayTick for the blocks that make ambient particles. Like vanilla, 1000 random positions within 16 blocks of the player are visited each tick
void Renderer::randomDisplayTicks(const World* world) {
    if (world == nullptr || mc->player == nullptr) return;
    const int centerX = static_cast<int>(std::floor(mc->player->position.x));
    const int centerY = static_cast<int>(std::floor(mc->player->position.y));
    const int centerZ = static_cast<int>(std::floor(mc->player->position.z));
    std::uniform_int_distribution<int> spread(0, 15);
    using glm::dvec3;
    for (int attempt = 0; attempt < 1000; ++attempt) {
        const int x = centerX + spread(generator()) - spread(generator());
        const int y = centerY + spread(generator()) - spread(generator());
        const int z = centerZ + spread(generator()) - spread(generator());
        if (y < 0 || y >= WORLD_HEIGHT) continue;
        const Block block = world->getBlockAt(x, y, z);
        const uint8_t meta = blockMeta(block);
        switch (blockId(block)) {
            case blockId(BLOCK_TORCH): case blockId(BLOCK_REDSTONE_TORCH): {
                // blockTorch.randomDisplayTick: smoke and a flame at the tip, leaning away from the wall
                double px = x + 0.5, py = y + 0.7, pz = z + 0.5;
                constexpr double lean = 0.27, rise = 0.22;
                switch (meta) {
                    case 1: px -= lean; py += rise; break;
                    case 2: px += lean; py += rise; break;
                    case 3: pz -= lean; py += rise; break;
                    case 4: pz += lean; py += rise; break;
                    default: break;
                }
                if (blockId(block) == blockId(BLOCK_TORCH)) {
                    createParticle(11, {px, py, pz}, {0.0, 0.0, 0.0}, nullptr);
                    createParticle(26, {px, py, pz}, {0.0, 0.0, 0.0}, nullptr);
                } else { // lit redstone torch: red dust
                    createParticle(30, {px + (uniform() - 0.5) * 0.2, py + (uniform() - 0.5) * 0.2,
                        pz + (uniform() - 0.5) * 0.2}, {0.0, 0.0, 0.0}, nullptr);
                }
                break;
            }
            case blockId(BLOCK_LIT_FURNACE): {
                // blockFurnace.randomDisplayTick: smoke and flame in front of the fire box
                const double px = x + 0.5, py = y + uniform() * 6.0 / 16.0, pz = z + 0.5;
                const double along = uniform() * 0.6 - 0.3, front = 0.52;
                dvec3 at(px, py, pz);
                switch (meta) {
                    case 4: at = {px - front, py, pz + along}; break; // facing west
                    case 5: at = {px + front, py, pz + along}; break; // east
                    case 2: at = {px + along, py, pz - front}; break; // north
                    case 3: at = {px + along, py, pz + front}; break; // south
                    default: break;
                }
                createParticle(11, at, {0.0, 0.0, 0.0}, nullptr);
                createParticle(26, at, {0.0, 0.0, 0.0}, nullptr);
                break;
            }
            case blockId(BLOCK_FIRE):
                if (uniform() < 0.25f)
                    createParticle(12, {x + static_cast<double>(uniform()), y + uniform() * 0.5 + 0.5,
                        z + static_cast<double>(uniform())}, {0.0, 0.0, 0.0}, nullptr);
                break;
            case blockId(BLOCK_LAVA): case blockId(BLOCK_FLOWING_LAVA):
                if (blockId(world->getBlockAt(x, y + 1, z)) == blockId(BLOCK_AIR) && uniform() < 0.05f)
                    createParticle(27, {x + static_cast<double>(uniform()), y + 1.0, z + static_cast<double>(uniform())},
                        {0.0, 0.0, 0.0}, nullptr);
                break;
            case blockId(BLOCK_LIT_REDSTONE_ORE): {
                // blockRedstoneOre.spawnParticles: dust on the faces that are open
                static constexpr int faces[6][3]{{0, 1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1}, {1, 0, 0}, {-1, 0, 0}};
                for (const auto& face : faces) {
                    if (BlockUtil::isFullBlock(world->getBlockAt(x + face[0], y + face[1], z + face[2]))) continue;
                    const double px = face[0] == 0 ? x + uniform() : x + 0.5 + face[0] * 0.5625;
                    const double py = face[1] == 0 ? y + uniform() : y + 0.5 + face[1] * 0.5625;
                    const double pz = face[2] == 0 ? z + uniform() : z + 0.5 + face[2] * 0.5625;
                    createParticle(30, {px, py, pz}, {0.0, 0.0, 0.0}, nullptr);
                }
                break;
            }
            case blockId(BLOCK_MYCELIUM):
                if (uniform() < 0.1f)
                    createParticle(22, {x + static_cast<double>(uniform()), y + 1.1, z + static_cast<double>(uniform())},
                        {0.0, 0.0, 0.0}, nullptr);
                break;
            case blockId(BLOCK_BREWING_STAND):
                createParticle(11, {x + 0.4 + uniform() * 0.2, y + 0.7, z + 0.4 + uniform() * 0.2}, {0.0, 0.0, 0.0}, nullptr);
                break;
            case blockId(BLOCK_ENCHANTING_TABLE): {
                // runes fly from the bookshelves around the table
                for (int dx = -2; dx <= 2; ++dx) {
                    for (int dz = -2; dz <= 2; ++dz) {
                        if (dx > -2 && dx < 2 && dz == -1) dz = 2;
                        if (uniform() > 0.0625f) continue;
                        for (int dy = 0; dy <= 1; ++dy) {
                            if (blockId(world->getBlockAt(x + dx, y + dy, z + dz)) != blockId(BLOCK_BOOKSHELF)) continue;
                            if (blockId(world->getBlockAt(x + dx / 2, y, z + dz / 2)) != blockId(BLOCK_AIR)) break;
                            createParticle(25, {x + 0.5, y + 2.0, z + 0.5},
                                {dx + uniform() - 0.5, dy - uniform() - 1.0, dz + uniform() - 0.5}, nullptr);
                        }
                    }
                }
                break;
            }
            case blockId(BLOCK_PORTAL):
                for (int index = 0; index < 4 && uniform() < 0.05f; ++index) {
                    const double px = x + uniform(), py = y + uniform(), pz = z + uniform();
                    double vx = (uniform() - 0.5) * 0.5, vy = (uniform() - 0.5) * 0.5, vz = (uniform() - 0.5) * 0.5;
                    const int direction = static_cast<int>(uniform() * 2.0f) * 2 - 1;
                    if (blockId(world->getBlockAt(x - 1, y, z)) == blockId(BLOCK_PORTAL)
                        || blockId(world->getBlockAt(x + 1, y, z)) == blockId(BLOCK_PORTAL)) {
                        vz = uniform() * 2.0 * direction;
                        createParticle(24, {x + 0.5 + 0.25 * direction, py, pz}, {vz, vy, vx}, nullptr);
                    } else {
                        vx = uniform() * 2.0 * direction;
                        createParticle(24, {px, py, z + 0.5 + 0.25 * direction}, {vx, vy, vz}, nullptr);
                    }
                }
                break;
            default: break;
        }
    }
}

void Renderer::tickParticles(const World* world) {
    tickBlockEntities();
    randomDisplayTicks(world);
    {
        // critical hit bursts: 16 attempts per tick inside the entity's box, for three ticks
        std::vector<ParticleEmitter> active;
        {
            std::lock_guard lock(emitterMutex);
            active.swap(emitters);
        }
        std::vector<ParticleEmitter> remaining;
        for (ParticleEmitter& emitter : active) {
            if (const std::shared_ptr<Entity> entity = world != nullptr ? world->getEntityById(emitter.entityId) : nullptr) {
                for (int attempt = 0; attempt < 16; ++attempt) {
                    const double dx = signedUnit(), dy = signedUnit(), dz = signedUnit();
                    if (dx * dx + dy * dy + dz * dz > 1.0) continue;
                    createParticle(emitter.type,
                        {entity->position.x + dx * entity->width / 4.0,
                         entity->position.y + entity->height / 2.0 + dy * entity->height / 4.0,
                         entity->position.z + dz * entity->width / 4.0},
                        {dx, dy + 0.2, dz}, nullptr);
                }
            }
            if (++emitter.age < 3) remaining.push_back(emitter);
        }
        std::lock_guard lock(emitterMutex);
        emitters.insert(emitters.end(), remaining.begin(), remaining.end());
    }
    {
        // server particles (S2A): count 0 is a single particle moving by offset * speed, otherwise `count` particles scattered by a gaussian with the offset as its spread
        std::vector<PendingParticle> pending;
        {
            std::lock_guard lock(pendingParticleMutex);
            pending.swap(pendingParticles);
        }
        std::normal_distribution<double> gaussian(0.0, 1.0);
        for (const PendingParticle& request : pending) {
            if (request.count == 0) {
                createParticle(request.type, request.position, request.offset * static_cast<double>(request.speed),
                    request.arguments);
                continue;
            }
            for (int index = 0; index < request.count; ++index) {
                const glm::dvec3 scatter(gaussian(generator()) * request.offset.x,
                    gaussian(generator()) * request.offset.y, gaussian(generator()) * request.offset.z);
                const glm::dvec3 motion(gaussian(generator()), gaussian(generator()), gaussian(generator()));
                createParticle(request.type, request.position + scatter, motion * static_cast<double>(request.speed),
                    request.arguments);
            }
        }
    }
    const size_t existing = particles.size();
    for (size_t index = 0; index < existing; ++index) {
        Particle& particle = particles[index];
        particle.previousPosition = particle.position;
        updateParticle(particle, world);
    }
    std::erase_if(particles, [](const Particle& particle) {
        return particle.age >= particle.maximumAge;
    });
    mc->activeParticleCount.store(particles.size(), std::memory_order_relaxed);
}

void Renderer::renderParticles(const float partialTicks) const {
    if (particles.empty()) return;

    struct Vertex { glm::vec3 position; glm::vec2 uv; glm::vec3 color; float alpha; };
    std::vector<Vertex> batches[5];
    const glm::mat3 inverseView = glm::transpose(glm::mat3(view));
    const glm::vec3 right = inverseView[0];
    const glm::vec3 up = inverseView[1];
    static constexpr int corners[6] = {0, 1, 2, 2, 3, 0};
    static constexpr glm::vec2 cornerOffsets[4]{{-1, -1}, {1, -1}, {1, 1}, {-1, 1}};
    static constexpr glm::vec2 cornerUvs[4]{{0, 1}, {1, 1}, {1, 0}, {0, 0}};
    constexpr float tileScale = 1.0f / atlasTilesPerRow;
    constexpr float fragmentScale = tileScale / 4.0f;

    for (const Particle& particle : particles) {
        if (particle.kind == KIND_HUGE_EXPLODE) continue;
        const glm::vec3 center = glm::vec3(glm::mix(particle.previousPosition, particle.position,
            static_cast<double>(partialTicks))) - cameraPos;
        const float progress = (static_cast<float>(particle.age) + partialTicks)
            / static_cast<float>(std::max(particle.maximumAge, 1));
        float brightness = 1.0f;
        if (const std::shared_ptr<Chunk> chunk = mc->world->getLoadedChunkAt(
                particle.position.x, particle.position.z)) {
            const int y = std::clamp(static_cast<int>(particle.position.y), 0, WORLD_HEIGHT - 1);
            const uint32_t light = chunk->getLightData(World::toLocalX(particle.position.x), y,
                World::toLocalZ(particle.position.z));
            const float level = static_cast<float>(std::max(light & 15U, (light >> 4U) & 15U)) / 15.0f;
            brightness = level / ((1.0f - level) * 3.0f + 1.0f);
        }
        // per kind brightness and size curves, as in the vanilla renderParticle/getBrightness overrides
        float size = particle.size;
        float alpha = particle.alpha;
        const float grow = std::clamp(progress * 32.0f, 0.0f, 1.0f);
        switch (particle.kind) {
            case KIND_FLAME:
                size = particle.baseSize * (1.0f - progress * progress * 0.5f);
                brightness = brightness * std::clamp(progress, 0.0f, 1.0f) + (1.0f - std::clamp(progress, 0.0f, 1.0f));
                break;
            case KIND_SMOKE: case KIND_CLOUD: case KIND_REDDUST: case KIND_NOTE: case KIND_HEART:
            case KIND_SPELL: case KIND_CRIT:
                size = particle.baseSize * grow;
                break;
            case KIND_LAVA:
                size = particle.baseSize * (1.0f - progress * progress);
                brightness = 1.0f;
                break;
            case KIND_PORTAL: case KIND_ENCHANT: {
                const float fade = std::clamp(static_cast<float>(particle.age) / static_cast<float>(particle.maximumAge), 0.0f, 1.0f);
                const float p4 = fade * fade * fade * fade;
                brightness = brightness * (1.0f - p4) + p4;
                if (particle.kind == KIND_PORTAL) {
                    float f = 1.0f - progress;
                    f = 1.0f - f * f;
                    size = particle.baseSize * f;
                }
                break;
            }
            case KIND_LARGE_EXPLODE: brightness = 1.0f; break;
            case KIND_FOOTSTEP: {
                const float f = progress * progress;
                alpha = std::min(2.0f - f * 2.0f, 1.0f) * 0.2f;
                break;
            }
            default: break;
        }
        const glm::vec3 color = particle.color * brightness * particle.colorScale;
        std::vector<Vertex>& batch = batches[std::min<int>(particle.sheet, 4)];

        if (particle.kind == KIND_FOOTSTEP) { // a flat quad on the ground
            for (const int corner : corners) {
                const glm::vec2 offset = cornerOffsets[corner] * size;
                batch.push_back({center + glm::vec3(offset.x, 0.005f, offset.y),
                    cornerUvs[corner], glm::vec3(brightness), alpha});
            }
            continue;
        }
        if (particle.sheet == 4) { // large explosion: frame of a 4x4 sheet
            const int frame = std::min(static_cast<int>(progress * 15.0f), 15);
            const glm::vec2 origin(static_cast<float>(frame % 4) / 4.0f, static_cast<float>(frame / 4) / 4.0f);
            for (const int corner : corners) {
                const glm::vec2 offset = cornerOffsets[corner] * size;
                batch.push_back({center + right * offset.x + up * offset.y,
                    origin + cornerUvs[corner] * 0.24975f, particle.color, 1.0f});
            }
            continue;
        }
        for (const int corner : corners) {
            const glm::vec2 offset = cornerOffsets[corner] * size;
            glm::vec2 uv;
            if (particle.sheet == 0 || particle.sheet == 2) {
                const int tileX = particle.tile % atlasTilesPerRow;
                const int tileY = particle.tile / atlasTilesPerRow;
                uv = glm::vec2(tileX, tileY) * tileScale
                    + (glm::vec2(particle.textureX, particle.textureY) + cornerUvs[corner]) * fragmentScale;
            } else {
                const int spriteX = std::max(particle.sprite, 0) % 16;
                const int spriteY = std::max(particle.sprite, 0) / 16;
                uv = (glm::vec2(spriteX, spriteY) + cornerUvs[corner] * 0.99f) / 16.0f;
            }
            batch.push_back({center + right * offset.x + up * offset.y, uv, color, alpha});
        }
    }

    particleShader->use();
    particleShader->setMat4("viewProjection", glm::value_ptr(projection * view));
    particleShader->setInt("blockAtlas", 0);
    glActiveTexture(GL_TEXTURE0);
    glBindVertexArray(particleVAO);
    glBindBuffer(GL_ARRAY_BUFFER, particleVBO);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE);
    glDisable(GL_CULL_FACE);
    const GLuint textures[5]{blockAtlasTexture, particleSheetTexture, itemAtlasTexture, footprintTexture, explosionTexture};
    for (int sheet = 0; sheet < 5; ++sheet) {
        if (batches[sheet].empty()) continue;
        glBindTexture(GL_TEXTURE_2D, textures[sheet]);
        glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(batches[sheet].size() * sizeof(Vertex)),
            batches[sheet].data(), GL_STREAM_DRAW);
        glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(batches[sheet].size()));
    }
    glEnable(GL_CULL_FACE);
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
    glBindVertexArray(0);
}
