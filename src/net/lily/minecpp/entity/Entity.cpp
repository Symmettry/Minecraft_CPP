#include "Entity.hpp"
#include "net/lily/minecpp/Minecraft.hpp"
#include <cmath>

// please never let me touch this ever again this is so aids

Entity::Entity(const Minecraft* mc, double x, double y, double z)
    : mc(mc) {
    setPosition(x, y, z);
}

double Entity::getEyePos() const {
    return position.y + getEyeHeight();
}

double Entity::getLastEyePos() const {
    return lastPos.y + getEyeHeight();
}

void Entity::update() {
    lastPos = position;
    lastRot = rotation;
    onLivingUpdate();
}

void Entity::jump() {
    velocity.y = jumpHeight();
    if (isSprinting() && moveForward > 0.0) {
        double yawRad = rotation.yaw * M_PI / 180.0;
        double boost = 0.2;
        velocity.z += std::sin(yawRad) * boost;
        velocity.x += std::cos(yawRad) * boost;
    }
    isAirBorne = true;
}

bool Entity::isOnLadder() const {
    int i = floor(position.x);
    int j = floor(getBoundingBox().minY);
    int k = floor(position.z);
    const Block* block = mc->world->getBlockAt(i, j, k);
    // return net.minecraftforge.common.ForgeHooks.isLivingOnLadder(block, worldObj, new BlockPos(i, j, k), this);
    return false; // todo
}

void Entity::onLivingUpdate() {
    if (jumpTicks > 0) --jumpTicks;
    moveStrafe *= 0.98;
    moveForward *= 0.98;

    if (isJumping && onGround && jumpTicks == 0) {
        jump();
        jumpTicks = 10;
    }

    moveEntityWithHeading(moveStrafe, moveForward);

    if (std::abs(velocity.x) < 1e-5) velocity.x = 0.0;
    if (std::abs(velocity.y) < 1e-5) velocity.y = 0.0;
    if (std::abs(velocity.z) < 1e-5) velocity.z = 0.0;
}

const Block* Entity::getBlockBelow() const {
    return mc->world->getBlockAt(std::floor(position.x), static_cast<int>(std::floor(getBoundingBox().minY)) - 1, std::floor(position.z));
}

void Entity::moveEntityWithHeading(float strafe, float forward) {
    if (isInLava()) {
        double d1 = position.y;
        moveFlying(strafe, forward, 0.02f);
        velocity.x *= 0.5;
        velocity.y *= 0.5;
        velocity.z *= 0.5;
        velocity.y -= 0.02;

        // some other thing todo
        return;
    }
    if (isInWater()) {
        double d0 = position.y;
        float f1 = 0.8F;
        float f2 = 0.02F;
        // float f3 = static_cast<float>(EnchantmentHelper.getDepthStriderModifier(this));
        float f3 = 0.0f;

        if (f3 > 3.0F)
        {
            f3 = 3.0F;
        }

        if (!onGround)
        {
            f3 *= 0.5F;
        }

        if (f3 > 0.0F)
        {
            f1 += (0.54600006F - f1) * f3 / 3.0F;
            f2 += (getAIMoveSpeed() * 1.0F - f2) * f3 / 3.0F;
        }

        moveFlying(strafe, forward, f2);
        moveEntity(velocity.x, velocity.y, velocity.z);
        velocity.x *= static_cast<double>(f1);
        velocity.y *= 0.800000011920929;
        velocity.z *= static_cast<double>(f1);
        velocity.y -= 0.02;

        // some other thing todo
        return;
    }

    float f4 = 0.91f;

    if (onGround) {
        f4 = getBlockBelow()->slipperiness() * 0.91f;
    }

    float f = 0.16277136F / (f4 * f4 * f4);
    float f5 = onGround ? getAIMoveSpeed() * f : jumpMovementFactor;

    if (isSprinting()) f5 *= 1.3f;

    moveFlying(strafe, forward, f5);
    f4 = 0.91f;

    if (onGround) {
        f4 = getBlockBelow()->slipperiness() * 0.91f;
    }

    if (isOnLadder()) {
        float f6 = 0.15f;
        velocity.x = std::clamp(velocity.x, static_cast<double>(-f6), static_cast<double>(f6));
        velocity.z = std::clamp(velocity.z, static_cast<double>(-f6), static_cast<double>(f6));
        fallDistance = 0.0f;

        if (velocity.y < -0.15) {
            velocity.y = -0.15;
        }

        bool flag = isSneaking() && dynamic_cast<EntityPlayer*>(this);
        if (flag && velocity.y < 0.0) {
            velocity.y = 0.0;
        }
    }

    moveEntity(velocity.x, velocity.y, velocity.z);

    if (isCollidedHorizontally && isOnLadder()) {
        velocity.y = 0.2;
    }

    // if not loaded bla bla bla

    velocity.y -= 0.08;
    velocity.y *= 0.98f;
    velocity.x *= static_cast<double>(f4);
    velocity.z *= static_cast<double>(f4);
}

void Entity::moveFlying(float strafe, float forward, const float friction) const {
    float f = strafe * strafe + forward * forward;
    if (f < 1e-4) return;
    f = std::sqrt(f);
    if (f < 1.0f) {
        f = 1.0f;
    }
    f = friction / f;
    strafe *= f;
    forward *= f;

    float f1 = std::sin(rotation.yaw * static_cast<float>(M_PI / 180.0f));
    float f2 = std::cos(rotation.yaw * static_cast<float>(M_PI / 180.0f));
    velocity.x += forward * f2 - strafe * f1;
    velocity.z += forward * f1 + strafe * f2;
}

void Entity::moveEntity(double dx, double dy, double dz) {
    if (noClip) {
        boundingBox = getBoundingBox().offset(dx, dy, dz);
        resetPositionToBB();
        return;
    }

    double d0 = position.x;
    double d1 = position.y;
    double d2 = position.z;

    if (isInWeb)
    {
        isInWeb = false;
        dx *= 0.25;
        dy *= 0.05000000074505806;
        dz *= 0.25;
        velocity.x = 0.0;
        velocity.y = 0.0;
        velocity.z = 0.0;
    }

    double d3 = dx;
    double d4 = dy;
    double d5 = dz;

    if (onGround && isSneaking() && dynamic_cast<EntityPlayer*>(this)) {

        double d6;
        for (d6 = 0.05; dx != 0.0 && mc->world->getCollidingBlocks(getBoundingBox().offset(dx, -1.0, 0.0)).empty(); d3 = dx)
        {
            if (dx < d6 && dx >= -d6)
            {
                dx = 0.0;
            }
            else if (dx > 0.0)
            {
                dx -= d6;
            }
            else
            {
                dx += d6;
            }
        }

        for (; dz != 0.0 && mc->world->getCollidingBlocks(getBoundingBox().offset(0.0, -1.0, dz)).empty(); d5 = dz)
        {
            if (dz < d6 && dz >= -d6)
            {
                dz = 0.0;
            }
            else if (dz > 0.0)
            {
                dz -= d6;
            }
            else
            {
                dz += d6;
            }
        }

        for (; dx != 0.0 && dz != 0.0 && mc->world->getCollidingBlocks(getBoundingBox().offset(dx, -1.0, dz)).empty(); d5 = dz)
        {
            if (dx < d6 && dx >= -d6)
            {
                dx = 0.0;
            }
            else if (dx > 0.0)
            {
                dx -= d6;
            }
            else
            {
                dx += d6;
            }

            d3 = dx;

            if (dz < d6 && dz >= -d6)
            {
                dz = 0.0;
            }
            else if (dz > 0.0)
            {
                dz -= d6;
            }
            else
            {
                dz += d6;
            }
        }
    }

    auto list1 = mc->world->getCollidingBlocks(getBoundingBox().expand(dx, dy, dz));
    auto axisalignedbb = getBoundingBox();

    for (auto axisalignedbb1 : list1)
    {
        dy = axisalignedbb1->calculateYOffset(getBoundingBox(), dy);
    }

    setBoundingBox(getBoundingBox().offset(0.0, dy, 0.0));
    bool flag1 = onGround || d4 != dy && d4 < 0.0;

    for (auto axisalignedbb2 : list1)
    {
        dx = axisalignedbb2->calculateXOffset(getBoundingBox(), dx);
    }

    setBoundingBox(getBoundingBox().offset(dx, 0.0, 0.0));

    for (auto axisalignedbb13 : list1)
    {
        dz = axisalignedbb13->calculateZOffset(getBoundingBox(), dz);
    }

    setBoundingBox(getBoundingBox().offset(0.0, 0.0, dz));

    if (stepHeight > 0.0F && flag1 && (d3 != dx || d5 != dz))
    {
        double d11 = dx;
        double d7 = dy;
        double d8 = dz;
        auto axisalignedbb3 = getBoundingBox();
        boundingBox = axisalignedbb;
        dy = static_cast<double>(stepHeight);
        auto list = mc->world->getCollidingBlocks(getBoundingBox().offset(d3, dy, d5));
        auto axisalignedbb4 = getBoundingBox();
        auto axisalignedbb5 = axisalignedbb4.expand(d3, 0.0, d5);
        double d9 = dy;

        for (auto axisalignedbb6 : list)
        {
            d9 = axisalignedbb6->calculateYOffset(axisalignedbb5, d9);
        }

        axisalignedbb4 = axisalignedbb4.offset(0.0, d9, 0.0);
        double d15 = d3;

        for (auto axisalignedbb7 : list)
        {
            d15 = axisalignedbb7->calculateXOffset(axisalignedbb4, d15);
        }

        axisalignedbb4 = axisalignedbb4.offset(d15, 0.0, 0.0);
        double d16 = d5;

        for (auto axisalignedbb8 : list)
        {
            d16 = axisalignedbb8->calculateZOffset(axisalignedbb4, d16);
        }

        axisalignedbb4 = axisalignedbb4.offset(0.0, 0.0, d16);
        auto axisalignedbb14 = getBoundingBox();
        double d17 = dy;

        for (auto axisalignedbb9 : list)
        {
            d17 = axisalignedbb9->calculateYOffset(axisalignedbb14, d17);
        }

        axisalignedbb14 = axisalignedbb14.offset(0.0, d17, 0.0);
        double d18 = d3;

        for (auto axisalignedbb10 : list)
        {
            d18 = axisalignedbb10->calculateXOffset(axisalignedbb14, d18);
        }

        axisalignedbb14 = axisalignedbb14.offset(d18, 0.0, 0.0);
        double d19 = d5;

        for (auto axisalignedbb11 : list)
        {
            d19 = axisalignedbb11->calculateZOffset(axisalignedbb14, d19);
        }

        axisalignedbb14 = axisalignedbb14.offset(0.0, 0.0, d19);
        double d20 = d15 * d15 + d16 * d16;
        double d10 = d18 * d18 + d19 * d19;

        if (d20 > d10)
        {
            dx = d15;
            dz = d16;
            dy = -d9;
            setBoundingBox(axisalignedbb4);
        }
        else
        {
            dx = d18;
            dz = d19;
            dy = -d17;
            setBoundingBox(axisalignedbb14);
        }

        for (auto axisalignedbb12 : list)
        {
            dy = axisalignedbb12->calculateYOffset(getBoundingBox(), dy);
        }

        setBoundingBox(getBoundingBox().offset(0.0, dy, 0.0));

        if (d11 * d11 + d8 * d8 >= dx * dx + dz * dz)
        {
            dx = d11;
            dy = d7;
            dz = d8;
            setBoundingBox(axisalignedbb3);
        }
    }

    resetPositionToBB();
    isCollidedHorizontally = d3 != dx || d5 != dz;
    isCollidedVertically = d4 != dy;
    onGround = isCollidedVertically && d4 < 0.0;
    isCollided = isCollidedVertically || isCollidedHorizontally;

    // updateFallState(...);

    if (d3 != dx)
    {
        velocity.x = 0.0;
    }

    if (d5 != dz)
    {
        velocity.z = 0.0;
    }

    if (d4 != dy) {
        velocity.y = 0.0;
    }

}

void Entity::resetPositionToBB() const {
    position.x = (boundingBox.minX + boundingBox.maxX) / 2;
    position.y = boundingBox.minY;
    position.z = (boundingBox.minZ + boundingBox.maxZ) / 2;
}

AABB Entity::getBoundingBox() const {
    return boundingBox;
}
void Entity::setBoundingBox(const AABB &box) const {
    boundingBox = box;
}

void Entity::setPosition(double x, double y, double z) const {
    lastPos.x = x;
    lastPos.y = y;
    lastPos.z = z;
    position.x = x;
    position.y = y;
    position.z = z;
    lastPos = position;
    velocity = {0.0, 0.0, 0.0};
    boundingBox = AABB{position.x - width/2, position.y, position.z - width/2, position.x + width/2, position.y + height, position.z + width/2};
}
