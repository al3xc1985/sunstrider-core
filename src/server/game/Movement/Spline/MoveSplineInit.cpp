/*
 * Copyright (C) 2005-2011 MaNGOS <http://getmangos.com/>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "MoveSplineInit.h"
#include "MoveSpline.h"
#include "MovementPacketBuilder.h"
#include "Unit.h"
#include "Transport.h"
#include "WorldPacket.h"
#include "Opcodes.h"

namespace Movement
{
    UnitMoveType SelectSpeedType(uint32 moveFlags)
    {
        if (moveFlags & MOVEMENTFLAG_CAN_FLY)
        {
            if (moveFlags & MOVEMENTFLAG_BACKWARD /*&& speed_obj.flight >= speed_obj.flight_back*/)
                return MOVE_FLIGHT_BACK;
            else
                return MOVE_FLIGHT;
        }
        else if (moveFlags & MOVEMENTFLAG_SWIMMING)
        {
            if (moveFlags & MOVEMENTFLAG_BACKWARD /*&& speed_obj.swim >= speed_obj.swim_back*/)
                return MOVE_SWIM_BACK;
            else
                return MOVE_SWIM;
        }
        else if (moveFlags & MOVEMENTFLAG_WALKING)
        {
            //if (speed_obj.run > speed_obj.walk)
            return MOVE_WALK;
        }
        else if (moveFlags & MOVEMENTFLAG_BACKWARD /*&& speed_obj.run >= speed_obj.run_back*/)
            return MOVE_RUN_BACK;

        // Flying creatures use MOVEMENTFLAG_DESCENDING or MOVEMENTFLAG_DISABLE_GRAVITY
        // Run speed is their default flight speed.
        return MOVE_RUN;
    }

    // send to player having this unit in sight, according to their client version. Will build packet for version only if needed
    void SendLaunchToSet(Unit* unit, bool transport)
    {
        WorldPacket data(SMSG_MONSTER_MOVE, 64);
        data << unit->GetPackGUID();
        if (transport)
        {
            data.SetOpcode(SMSG_MONSTER_MOVE_TRANSPORT);
            data.appendPackGUID(unit->GetTransGUID());
#ifdef LICH_KING
            packet << int8(unit->GetTransSeat());
#endif
        }

        PacketBuilder::WriteMonsterMove(*(unit->movespline), data);
        unit->SendMessageToSet(&data, true);
    }

    int32 MoveSplineInit::Launch()
    {
        MoveSpline& move_spline = *unit->movespline;

        bool transport = unit->HasUnitMovementFlag(MOVEMENTFLAG_ONTRANSPORT) && unit->GetTransGUID();
        Location real_position;
        // there is a big chance that current position is unknown if current state is not finalized, need compute it
        // this also allows CalculatePath spline position and update map position in much greater intervals
        // Don't compute for transport movement if the unit is in a motion between two transports
        if (!move_spline.Finalized() && move_spline.onTransport == transport)
            real_position = move_spline.ComputePosition();
        else
        {
            Position const* pos;
            if (!transport)
                pos = unit;
            else
                pos = &unit->m_movementInfo.transport.pos;

            real_position.x = pos->GetPositionX();
            real_position.y = pos->GetPositionY();
            real_position.z = pos->GetPositionZ();
            real_position.orientation = unit->GetOrientation();
        }

        // should i do the things that user should do? - no.
        if (args.path.empty())
            return 0;

        // corrent first vertex
        args.path[0] = real_position;
        args.initialOrientation = real_position.orientation;
        move_spline.onTransport = transport;

        uint32 moveFlags = unit->m_movementInfo.GetMovementFlags();
        moveFlags |= MOVEMENTFLAG_SPLINE_ENABLED;
        
#ifdef LICH_KING
        if (!args.flags.backward)
            moveFlags = (moveFlags & ~(MOVEMENTFLAG_BACKWARD)) | MOVEMENTFLAG_FORWARD;
        else
            moveFlags = (moveFlags & ~(MOVEMENTFLAG_FORWARD)) | MOVEMENTFLAG_BACKWARD;
#else
        moveFlags = (moveFlags & ~(MOVEMENTFLAG_BACKWARD)) | MOVEMENTFLAG_FORWARD;
#endif

        if (moveFlags & MOVEMENTFLAG_ROOT)
            moveFlags &= ~MOVEMENTFLAG_MASK_MOVING;

        if (!args.HasVelocity)
        {
            // If spline is initialized with SetWalk method it only means we need to select
            // walk move speed for it but not add walk flag to unit
            uint32 moveFlagsForSpeed = moveFlags;
            if (args.walk)
                moveFlagsForSpeed |= MOVEMENTFLAG_WALKING;
            else
                moveFlagsForSpeed &= ~MOVEMENTFLAG_WALKING;

            args.velocity = unit->GetSpeed(SelectSpeedType(moveFlagsForSpeed));
            //DEBUG_ASSERT(args.velocity);
        }

        if (!args.Validate(unit))
        {
            //kelno: make sure this flag is correctly removed if it's already there when entering this function.
            unit->m_movementInfo.RemoveMovementFlag(MOVEMENTFLAG_SPLINE_ENABLED);
            return 0;
        }

        unit->m_movementInfo.SetMovementFlags(moveFlags);
        move_spline.Initialize(args);

        SendLaunchToSet(unit, transport);

        return move_spline.Duration();
    }

    void MoveSplineInit::Stop()
    {
        MoveSpline& move_spline = *unit->movespline;

        // No need to stop if we are not moving
        if (move_spline.Finalized())
            return;

        bool transport = unit->HasUnitMovementFlag(MOVEMENTFLAG_ONTRANSPORT) && unit->GetTransGUID();
        Location loc;
        if (move_spline.onTransport == transport)
            loc = move_spline.ComputePosition();
        else
        {
            Position const* pos;
            if (!transport)
                pos = unit;
            else
                pos = &unit->m_movementInfo.transport.pos;

            loc.x = pos->GetPositionX();
            loc.y = pos->GetPositionY();
            loc.z = pos->GetPositionZ();
            loc.orientation = unit->GetOrientation();
        }

        args.flags = MoveSplineFlag::Done;
        unit->m_movementInfo.RemoveMovementFlag(MOVEMENTFLAG_FORWARD | MOVEMENTFLAG_SPLINE_ENABLED);
        move_spline.onTransport = transport;
        move_spline.Initialize(args);

        WorldPacket data(SMSG_MONSTER_MOVE, 64);
        data << unit->GetPackGUID();
        if (transport)
        {
            data.SetOpcode(SMSG_MONSTER_MOVE_TRANSPORT);
            data.appendPackGUID(unit->GetTransGUID());
#ifdef LICH_KING
            data << int8(unit->GetTransSeat());
#endif
        }

        // sunwell: increase z position in packet
        loc.z += unit->GetHoverOffset();

        PacketBuilder::WriteStopMovement(loc, args.splineId, data);
        unit->SendMessageToSet(&data, true);
        //SendStopToSet(unit, transport, loc, args.splineId);
    }

    MoveSplineInit::MoveSplineInit(Unit* m) : unit(m)
    {
        args.splineId = splineIdGen.NewId();
        // Elevators also use MOVEMENTFLAG_ONTRANSPORT but we do not keep track of their position changes
        args.TransformForTransport = unit->HasUnitMovementFlag(MOVEMENTFLAG_ONTRANSPORT) && unit->GetTransGUID();
        // mix existing state into new
#ifdef LICH_KING
        args.flags.canswim = unit->CanSwim();
#endif
        args.walk = unit->HasUnitMovementFlag(MOVEMENTFLAG_WALKING);

        args.flags.flying = unit->m_movementInfo.HasMovementFlag(MOVEMENTFLAG_CAN_FLY | MOVEMENTFLAG_DISABLE_GRAVITY);
    }

    void MoveSplineInit::SetFacing(const Unit* target)
    {
        args.flags.EnableFacingTarget();
        args.facing.target = target->GetGUID();
    }

    void MoveSplineInit::SetFacing(float angle)
    {
        if (args.TransformForTransport)
        {
            if (Transport* transport = unit->GetTransport())
                angle -= transport->GetOrientation();
        }
        args.facing.angle = G3D::wrap(angle, 0.f, (float)G3D::twoPi());
        args.flags.EnableFacingAngle();
    }

    void MoveSplineInit::MoveTo(const Vector3& dest, bool generatePath, bool forceDestination)
    {
        if (generatePath)
        {
            PathGenerator path(unit);
            bool result = path.CalculatePath(dest.x, dest.y, dest.z, forceDestination);
            if (result && !(path.GetPathType() & PATHFIND_NOPATH))
            {
                MovebyPath(path.GetPath());
                return;
            }
        }

        args.path_Idx_offset = 0;
        args.path.resize(2);
        TransportPathTransform transform(unit, args.TransformForTransport);
        args.path[1] = transform(dest);
    }
    /*
    void MoveSplineInit::MoveTo(const Vector3& dest, const Vector3& nextDestination, bool forceDestination)
    {
        //generate path to destination
        PathGenerator path(unit);
        bool result = path.CalculatePath(dest.x, dest.y, dest.z, forceDestination);

         //if we fail generating it, default to straight line
        if(!result || (path.GetPathType() & PATHFIND_NOPATH))
        {
            args.path_Idx_offset = 0;
            args.path.resize(2);
            TransportPathTransform transform(unit, args.TransformForTransport);
            args.path[1] = transform(dest);
            return;
        }

        //generate path from destination to next point
        const Position nextPos { nextDestination.x, nextDestination.y, nextDestination.z };
      //  PathGenerator nextPath(nextPos,unit->GetMapId(),unit->GetInstanceId(),path.GetOptions()); //use same options here
        //result = nextPath.CalculatePath(nextDestination.x, nextDestination.y, nextDestination.z, forceDestination);

        // if failed generating second path, return to default behavior
        //if (!result || (nextPath.GetPathType() & PATHFIND_NOPATH)) 
       // {
        //    MovebyPath(path.GetPath());
        //    return;
        //}
        //we succeeded to generate both path, lets contact those, without nextPath first point
    //    Movement::PointsArray newPath;
        //newPath.reserve(path.GetPath().size() + nextPath.GetPath().size()-1 );
     //   newPath.reserve(path.GetPath().size() +1 );
     //   newPath.insert(newPath.end(),path.GetPath().begin(),path.GetPath().end());
        //newPath.insert(newPath.end(),nextPath.GetPath().begin()+1,nextPath.GetPath().end());
     //   G3D::Vector3 vec { nextPos.m_positionX, nextPos.m_positionY, nextPos.m_positionZ };
     //   newPath.push_back(vec);

     //   //we're good to go !
      //  MovebyPath(newPath);
   // }
     * */

    Vector3 TransportPathTransform::operator()(Vector3 input)
    {
        if (_transformForTransport)
            if (TransportBase* transport = _owner->GetTransport())
                transport->CalculatePassengerOffset(input.x, input.y, input.z);

        return input;
    }
} // namespace Movement
