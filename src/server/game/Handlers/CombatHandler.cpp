/*
 * This file is part of the TrinityCore Project. See AUTHORS file for Copyright information
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "WorldSession.h"
#include "CombatPackets.h"
#include "Common.h"
#include "CreatureAI.h"
#include "DBCStructure.h"
#include "Log.h"
#include "ObjectAccessor.h"
#include "Player.h"
#include "Vehicle.h"
#include "WorldPacket.h"

void WorldSession::HandleAttackSwingOpcode(WorldPackets::Combat::AttackSwing& packet)
{
    // INFO:
    // 该处理函数是玩家开始进入战斗姿态，但并不意味着直接就开始攻击，只有
    // 当目标进入自己的攻击范围后则开始攻击

    Unit* enemy = ObjectAccessor::GetUnit(*_player, packet.Victim);

    // 检查是否能够获取到敌人object
    if (!enemy)
    {
        // stop attack state at client
        SendAttackStop(nullptr);
        return;
    }

    // 下面的函数主要是做一些可见性的检查，距离的检查，法术效果的检查(如果使用的法术)
    if (!_player->IsValidAttackTarget(enemy))
    {
        // stop attack state at client
        SendAttackStop(enemy);
        return;
    }

    // 检查玩家是否在载具中且能否攻击，在载具的某些位置是允许攻击的
    //! Client explicitly checks the following before sending CMSG_ATTACK_SWING packet,
    //! so we'll place the same check here. Note that it might be possible to reuse this snippet
    //! in other places as well.
    if (Vehicle* vehicle = _player->GetVehicle())
    {
        VehicleSeatEntry const* seat = vehicle->GetSeatForPassenger(_player);
        ASSERT(seat);
        if (!(seat->Flags & VEHICLE_SEAT_FLAG_CAN_ATTACK))
        {
            SendAttackStop(enemy);
            return;
        }
    }

    // 进入攻击状态
    _player->Attack(enemy, true);
}

void WorldSession::HandleAttackStopOpcode(WorldPackets::Combat::AttackStop& /*packet*/)
{
    GetPlayer()->AttackStop();
}

void WorldSession::HandleSetSheathedOpcode(WorldPackets::Combat::SetSheathed& packet)
{
    // NOTE: 此处是用于设置武器的 `鞘化状态`，依据传入的 packet.CurrentSheathState 来决定武器是出鞘还是收起

    if (packet.CurrentSheathState >= MAX_SHEATH_STATE)
    {
        TC_LOG_ERROR("network", "Unknown sheath state {} ??", packet.CurrentSheathState);
        return;
    }

    _player->SetSheath(SheathState(packet.CurrentSheathState));
}

void WorldSession::SendAttackStop(Unit const* enemy)
{
    SendPacket(WorldPackets::Combat::SAttackStop(GetPlayer(), enemy).Write());
}
