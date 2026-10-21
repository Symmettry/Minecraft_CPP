#pragma once
#define GLFW_INCLUDE_NONE
#include <glad/glad.hpp>
#include <GLFW/glfw3.h>

#include "BlockAtlas.hpp"
#include "ItemAtlas.hpp"
#include "AnimatedAtlas.hpp"
#include "Camera.hpp"
#include "../world/World.hpp"
#include "Shader.hpp"
#include "net/lily/minecpp/util/FrustumPlanes.hpp"
#include "net/lily/minecpp/net/packets/play/server/EntityMetadata.hpp"
#include <limits>
#include <future>
#include <optional>
#include <vector>
#include <unordered_map>
#include <mutex>

class EntityPlayer;
class EntityItem;
class EntityMob;
class EntityOtherPlayerMP;

class Renderer {
public:
    GLuint blockAtlasTexture{}; // single atlas texture
    GLuint grassColorTexture{};
    GLuint foliageColorTexture{};
    Shader* blockShader{};
    Shader* skyShader{};
    Shader* celestialShader{};
    Shader* starShader{};
    Shader* damageShader{};
    Shader* selectionShader{};
    Shader* handShader{};
    Shader* particleShader{};
    Shader* itemShader{};
    Shader* itemSpriteShader{};
    Shader* entityShadowShader{};

    BlockAtlasData blockAtlas = BlockAtlas::loadAtlas("assets/blockatlas.png", "assets/blockatlas.dat.zst");
    ItemAtlas itemAtlas = ItemAtlas::load("assets/itematlas.dat.zst");
    std::unique_ptr<AnimatedAtlas> animatedAtlas;

    unsigned int cubeVAO{}, cubeVBO{};
    GLuint damageVAO{}, damageVBO{};
    // first vertex and count of a block item's mesh inside damageVAO: cube, stairs or half slab
    static std::pair<GLint, GLsizei> blockItemRange(const Block block) {
        if (BlockUtil::isStair(block)) return {48, 72};
        switch (blockId(block)) {
            case blockId(BLOCK_STONE_SLAB): case blockId(BLOCK_WOODEN_SLAB): case blockId(BLOCK_STONE_SLAB2):
                return {120, 36};
            default: return {0, 36};
        }
    }
    GLuint selectionVAO{}, selectionVBO{};
    GLuint stairSelectionVAO{}, stairSelectionVBO{};
    GLuint handVAO{}, handVBO{}, playerSkinTexture{}, alexSkinTexture{};
    GLuint playerVAO{}, playerVBO{}, slimPlayerVAO{}, slimPlayerVBO{};
    GLuint armorVAO{}, armorVBO{};
    // horses: base coats, marking overlays, and the non-horse types
    GLuint horseCoatTextures[7]{}, horseMarkingTextures[5]{};
    GLuint donkeyTexture{}, muleTexture{}, zombieHorseTexture{}, skeletonHorseTexture{};
    mutable std::unordered_map<std::string, GLuint> armorTextures;
    GLuint armorTexture(const std::string& name) const;
    GLuint particleVAO{}, particleVBO{};
    GLuint itemQuadVAO{}, itemQuadVBO{};
    struct ItemExtrusionMesh { GLuint vao{}, vbo{}; GLsizei vertexCount{}; };
    mutable std::unordered_map<uint16_t, ItemExtrusionMesh> itemExtrusionMeshCache;
    mutable std::unordered_map<uint32_t, ItemExtrusionMesh> blockAtlasExtrusionMeshCache;
    std::vector<unsigned char> itemAtlasPixels;
    int itemAtlasPixelWidth{}, itemAtlasPixelHeight{}, itemAtlasPixelChannels{};
    GLuint entityShadowVAO{}, entityShadowVBO{};
    GLuint celestialVAO{}, celestialVBO{}, starVAO{}, starVBO{};
    GLuint sunTexture{}, moonTexture{}, entityShadowTexture{};
    GLuint skullVAO{}, skullVBO{};
    GLuint skeletonSkullTexture{}, witherSkullTexture{}, zombieSkullTexture{}, creeperSkullTexture{};
    struct MobPart {
        int firstVertex{};
        glm::vec3 pivot{};
        glm::vec3 rotation{};
        uint8_t animation{};
        int parent{-1}; // index of the part whose transform this one inherits (pivot is then relative to it)
    };
    struct MobMesh {
        GLuint vao{}, vbo{}, texture{};
        std::vector<MobPart> parts;
    };
    std::unordered_map<uint16_t, MobMesh> mobMeshes;
    GLuint anvilVAO{}, anvilVBO{};
    GLsizei starCount{};
    mutable int fps{};
    mutable int renderedChunks{};
    mutable int totalChunks{};
    mutable size_t chunkUpdates{};
    mutable size_t renderedEntities{};
    mutable size_t totalEntities{};
    mutable size_t hiddenEntities{};

    Renderer(Minecraft* mc, Camera* camera, const int w, const int h) : window(nullptr), width(w), height(h), camera(camera), mc(mc) {}
    ~Renderer();

    void init();
    void render(const World *world) const;
    [[nodiscard]] bool shouldClose() const;

    void updateProjection(int fbWidth, int fbHeight, float fov) const;

    void updateVisibleChunks() const;

    void updateProjection(int fbWidth, int fbHeight) const;
    void updateProjection(float fov) const;

    void updateFrustums() const;

    void calculateView(float yawDegrees, float pitchDegrees) const;
    void renderSky(const World* world, float partialTicks) const;
    void renderBlockDamage(const World* world) const;
    void renderSelectionBox() const;
    void setupFog(const World* world, float partialTicks) const;
    void renderHand(float partialTicks) const;
    void updateEquippedItem();
    void renderPlayer(float partialTicks) const;
    void renderSkullBlocks(float partialTicks) const;
    // block entity models: chests and the enchanting table book. Instances are gathered from the per-chunk
    // lists the mesher fills, grouped by model so each texture and vertex array is bound once.
    void renderBlockEntityModels(float partialTicks) const;
    void tickBlockEntities() const;
    void setChestViewers(const glm::ivec3& position, int viewers) const;
    struct ChestState { int viewers = 0; float angle = 0.0f, previousAngle = 0.0f; };
    mutable std::unordered_map<uint64_t, ChestState> chestStates;
    mutable std::mutex chestStateMutex;
    struct BookState { float spread = 0.0f, rotation = 0.0f, flip = 0.0f; double lastTime = -1.0; };
    mutable std::unordered_map<uint64_t, BookState> bookStates;
    // s2A: queued from the network thread, spawned on the next particle tick
    void queueServerParticle(int type, const glm::dvec3& position, const glm::dvec3& offset, float speed,
                             int count, const int* arguments);
    void tickParticles(const World* world);
    // entityParticleEmitter: a burst of particles around an entity for three ticks (critical hits)
    void emitParticlesAtEntity(int entityId, int particleType);
    struct ParticleEmitter { int entityId; int type; int age; };
    std::mutex emitterMutex;
    std::vector<ParticleEmitter> emitters;
    void randomDisplayTicks(const World* world);
    void addBlockHitParticle(const glm::ivec3& position, uint8_t facing, Block block);
    void addBlockDestroyParticles(const glm::ivec3& position, Block block);
    void addBlockRunningParticle(const glm::dvec3 &position, const glm::dvec3 &velocity,
        float entityWidth, Block block);
    void addBlockLandingParticles(const glm::dvec3 &position, float entityWidth, Block block, int count);
    void initSky();

    [[nodiscard]] GLuint getItemAtlasTexture() const { return itemAtlasTexture; }
    void renderGuiBlockItem(Block block, float x, float y, float size) const;
    void renderGuiSkullItem(int damage, float x, float y, float size) const;
    void renderInventoryPlayer(float x, float y, float width, float height,
        float mouseX, float mouseY) const;

    static unsigned int loadTexture(const char *path);

    GLFWwindow* window;

    mutable int width, height;
    mutable bool shouldCalcView = true;

private:
    struct SkinImage {
        std::vector<unsigned char> pixels;
        int width = 0;
        int height = 0;
    };
    struct SkinTexture {
        GLuint texture = 0;
        bool failed = false;
        std::future<std::optional<SkinImage>> download;
    };
    mutable std::unordered_map<std::string, SkinTexture> skinTextures;
    std::pair<GLuint, bool> playerSkin(const EntityPlayer* player) const;
    static std::optional<SkinImage> downloadSkin(const std::string& url);
    void renderPlayerEntity(const EntityPlayer* player, float partialTicks) const;
    void renderEntityName(const Entity* entity, const std::string& name, float partialTicks) const;
    void renderItemEntity(const EntityItem* item, float partialTicks) const;
    void renderItemGlint(GLuint vao, GLint first, GLsizei count,
        const glm::mat4* models, int instances) const;
    void renderMobEntity(const EntityMob* mob, float partialTicks) const;
    glm::vec3 interpolatedPosition(const Entity* entity, float partialTicks) const;
    struct DisplayTransform { glm::vec3 rotation{}, translation{}, scale{1.0f}; };
    mutable std::unordered_map<std::string, DisplayTransform> thirdPersonTransformCache;
    const DisplayTransform& thirdPersonTransform(const std::string& name, bool blockModel) const;
    void renderHeldItemThirdPerson(const NetworkItemStack& stack, const glm::mat4& handMatrix,
                                   bool sneaking, float brightness) const;
    const ItemExtrusionMesh& getOrBuildItemExtrusionMesh(uint16_t tile) const;
    const ItemExtrusionMesh& getOrBuildBlockAtlasExtrusionMesh(int tileX, int tileY) const;
    ItemExtrusionMesh buildExtrusionMesh(const unsigned char* pixels, int pixelWidth, int pixelHeight,
        int originPxX, int originPxY, int iconSize) const;
    void queueEntityShadow(const Entity* entity, float partialTicks, float radius) const;
    void renderEntityShadows() const;
    struct Particle {
        glm::dvec3 previousPosition{};
        glm::dvec3 position{};
        glm::dvec3 velocity{};
        float size = 0.1f;
        float gravity = 1.0f;
        float collisionRadius = 0.1f;
        glm::vec3 color{1.0f};
        int age = 0;
        int maximumAge = 20;
        uint16_t tile = 0;
        float textureX = 0.0f;
        float textureY = 0.0f;
        // server particles (S2A): behavior, which sprite sheet to draw from and how to draw it
        uint8_t kind = 0;      // particleKind, 0 is the block-fragment particle
        uint8_t sheet = 0;     // 0 block atlas, 1 particles.png, 2 item atlas, 3 footprint, 4 explosion
        int sprite = 0;        // index into the 16x16 particles.png grid
        float alpha = 1.0f;
        float colorScale = 0.6f;  // block fragments are darkened, other particles are not
        bool noClip = false;
        float baseSize = 0.1f;    // size before the kind's growth/shrink curve
        glm::dvec3 anchor{};
        float extra = 0.0f;       // kind specific (explosion scale, portal motion, ...)
    };

    // a particle requested by the server, queued until the next client tick
    struct PendingParticle {
        int type = 0;
        glm::dvec3 position{};
        glm::dvec3 offset{};
        float speed = 0.0f;
        int count = 0;
        int arguments[2]{};
    };
    std::mutex pendingParticleMutex;
    std::vector<PendingParticle> pendingParticles;
    GLuint particleSheetTexture{}, footprintTexture{}, explosionTexture{};
    static Particle makeParticle(uint8_t kind, const glm::dvec3& position);
    void createParticle(int type, const glm::dvec3& position, const glm::dvec3& velocity, const int* arguments);
    void updateParticle(Particle& particle, const World* world);

    Camera* camera;
    Minecraft* mc;
    int chunkOffsetLoc = 0;
    void processInput() const;
    void renderParticles(float partialTicks) const;
    void addBlockParticle(glm::vec3 position, glm::vec3 velocity, Block block,
        float scale = 1.0f, float velocityMultiplier = 1.0f);

    mutable glm::mat4 projection = glm::mat4(0.0f),
                      view       = glm::mat4(0.0f),
                      projView   = glm::mat4(0.0f);

    mutable glm::vec3 cameraPos  = glm::vec3(0.0f);
    mutable std::vector<std::shared_ptr<Entity>> frameEntities;

    mutable bool freezeFrustum = false;
    mutable int appliedMipmapLevels = -1;
    mutable float appliedProjectionFov = std::numeric_limits<float>::quiet_NaN();

    mutable FrustumInfo frustumPlanes{};
    mutable uint64_t cachedChunkRevision = std::numeric_limits<uint64_t>::max();
    mutable std::vector<std::pair<ChunkCoord, std::shared_ptr<Chunk>>> cachedChunks;
    mutable std::vector<const std::pair<ChunkCoord, std::shared_ptr<Chunk>>*> translucentChunks;
    mutable glm::vec3 lastCullCameraPos{std::numeric_limits<float>::infinity()};
    mutable glm::vec2 lastCullCameraRotation{std::numeric_limits<float>::infinity()};
    mutable float lastCullFov = std::numeric_limits<float>::infinity();
    mutable int lastCullRenderDistance = -1;
    mutable glm::ivec2 lastCullCameraChunk{std::numeric_limits<int>::max()};
    mutable glm::vec3 lastTranslucentSortPos{std::numeric_limits<float>::infinity()};
    std::vector<Particle> particles;
    GLuint itemAtlasTexture = 0;
    GLuint itemGlintTexture = 0;
    Shader* itemGlintShader = nullptr;
    NetworkItemStack itemToRender{};
    float equippedProgress = 0.0f;
    float previousEquippedProgress = 0.0f;
    int equippedItemSlot = -1;
    mutable std::vector<float> entityShadowVertices;
};
