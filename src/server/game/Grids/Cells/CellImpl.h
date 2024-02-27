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

#ifndef TRINITY_CELLIMPL_H
#define TRINITY_CELLIMPL_H

#include <cmath>

#include "Cell.h"
#include "Map.h"
#include "Object.h"

inline Cell::Cell(CellCoord const& p) : data()
{
    data.Part.grid_x = p.x_coord / MAX_NUMBER_OF_CELLS;
    data.Part.grid_y = p.y_coord / MAX_NUMBER_OF_CELLS;
    data.Part.cell_x = p.x_coord % MAX_NUMBER_OF_CELLS;
    data.Part.cell_y = p.y_coord % MAX_NUMBER_OF_CELLS;
}

inline CellArea Cell::CalculateCellArea(float x, float y, float radius)
{
    if (radius <= 0.0f)
    {
        CellCoord center = Trinity::ComputeCellCoord(x, y).normalize();
        return CellArea(center, center);
    }

    CellCoord centerX = Trinity::ComputeCellCoord(x - radius, y - radius).normalize();
    CellCoord centerY = Trinity::ComputeCellCoord(x + radius, y + radius).normalize();

    return CellArea(centerX, centerY);
}

template<class T, class CONTAINER>
inline void Cell::Visit(CellCoord const& standing_cell, TypeContainerVisitor<T, CONTAINER>& visitor, Map& map, WorldObject const& obj, float radius) const
{
    //we should increase search radius by object's radius, otherwise
    //we could have problems with huge creatures, which won't attack nearest players etc
    Visit(standing_cell, visitor, map, obj.GetPositionX(), obj.GetPositionY(), radius + obj.GetCombatReach());
}

template<class T, class CONTAINER>
inline void Cell::Visit(CellCoord const& standing_cell, TypeContainerVisitor<T, CONTAINER>& visitor, Map& map, float x_off, float y_off, float radius) const
{
    // 计算初始单元格是否合法
    if (!standing_cell.IsCoordValid())
        return;

    // 如果 radius 可视范围小于 0，则直接访问当前单元格内的对象
    //no jokes here... Actually placing ASSERT() here was good idea, but
    //we had some problems with DynamicObjects, which pass radius = 0.0f (DB issue?)
    //maybe it is better to just return when radius <= 0.0f?
    if (radius <= 0.0f)
    {
        map.Visit(*this, visitor);
        return;
    }
    // 计算一些边界条件
    //lets limit the upper value for search radius
    if (radius > SIZE_OF_GRIDS)
        radius = SIZE_OF_GRIDS;

    // 依据 Object 所在的坐标(x_off, y_off)并结合可视范围radius计算其所在单元格其周边范围内的单元格形成一个区域，这里使用了
    // 最大顶点和最小顶点来表示一整个区域
    //lets calculate object coord offsets from cell borders.
    CellArea area = Cell::CalculateCellArea(x_off, y_off, radius);
    //if radius fits inside standing cell
    if (!area)
    {
        map.Visit(*this, visitor);
        return;
    }

    // NOTE: 避免区域过大，比如说计算出了一个大于 5x4 的范围区域，则需要进一步退化，改用访问圆心的方式进行 visit
    //
    // 算法的实现中，是先从从两个顶点的 x 轴坐标求 area 水平范围内的中间节点，然后从中间节点开始遍历 Y 轴，并在每次
    // 遍历完成后都减少Y轴的步伐(减1)，以此遍历完整个水平轴，形成一个近似圆的范围
    //
    //visit all cells, found in CalculateCellArea()
    //if radius is known to reach cell area more than 4x4 then we should call optimized VisitCircle
    //currently this technique works with MAX_NUMBER_OF_CELLS 16 and higher, with lower values
    //there are nothing to optimize because SIZE_OF_GRID_CELL is too big...
    if ((area.high_bound.x_coord > (area.low_bound.x_coord + 4)) && (area.high_bound.y_coord > (area.low_bound.y_coord + 4)))
    {
        VisitCircle(visitor, map, area.low_bound, area.high_bound);
        return;
    }

    // 先visit当前的单元格
    //ALWAYS visit standing cell first!!! Since we deal with small radiuses
    //it is very essential to call visitor for standing cell firstly...
    map.Visit(*this, visitor);

    // 然后visit范围内的其它单元格
    // loop the cell range
    for (uint32 x = area.low_bound.x_coord; x <= area.high_bound.x_coord; ++x)
    {
        for (uint32 y = area.low_bound.y_coord; y <= area.high_bound.y_coord; ++y)
        {
            CellCoord cellCoord(x, y);
            //lets skip standing cell since we already visited it
            if (cellCoord != standing_cell)
            {
                Cell r_zone(cellCoord);
                r_zone.data.Part.nocreate = this->data.Part.nocreate;
                map.Visit(r_zone, visitor);
            }
        }
    }
}

template<class T, class CONTAINER>
inline void Cell::VisitCircle(TypeContainerVisitor<T, CONTAINER>& visitor, Map& map, CellCoord const& begin_cell, CellCoord const& end_cell) const
{
    //here is an algorithm for 'filling' circum-squared octagon
    uint32 x_shift = (uint32)ceilf((end_cell.x_coord - begin_cell.x_coord) * 0.3f - 0.5f);
    //lets calculate x_start/x_end coords for central strip...
    const uint32 x_start = begin_cell.x_coord + x_shift;
    const uint32 x_end = end_cell.x_coord - x_shift;

    //visit central strip with constant width...
    for (uint32 x = x_start; x <= x_end; ++x)
    {
        for (uint32 y = begin_cell.y_coord; y <= end_cell.y_coord; ++y)
        {
            CellCoord cellCoord(x, y);
            Cell r_zone(cellCoord);
            r_zone.data.Part.nocreate = this->data.Part.nocreate;
            map.Visit(r_zone, visitor);
        }
    }

    //if x_shift == 0 then we have too small cell area, which were already
    //visited at previous step, so just return from procedure...
    if (x_shift == 0)
        return;

    uint32 y_start = end_cell.y_coord;
    uint32 y_end = begin_cell.y_coord;
    //now we are visiting borders of an octagon...
    for (uint32 step = 1; step <= (x_start - begin_cell.x_coord); ++step)
    {
        //each step reduces strip height by 2 cells...
        y_end += 1;
        y_start -= 1;
        for (uint32 y = y_start; y >= y_end; --y)
        {
            //we visit cells symmetrically from both sides, heading from center to sides and from up to bottom
            //e.g. filling 2 trapezoids after filling central cell strip...
            CellCoord cellCoord_left(x_start - step, y);
            Cell r_zone_left(cellCoord_left);
            r_zone_left.data.Part.nocreate = this->data.Part.nocreate;
            map.Visit(r_zone_left, visitor);

            //right trapezoid cell visit
            CellCoord cellCoord_right(x_end + step, y);
            Cell r_zone_right(cellCoord_right);
            r_zone_right.data.Part.nocreate = this->data.Part.nocreate;
            map.Visit(r_zone_right, visitor);
        }
    }
}

template<class T>
inline void Cell::VisitGridObjects(WorldObject const* center_obj, T& visitor, float radius, bool dont_load /*= true*/)
{
    CellCoord p(Trinity::ComputeCellCoord(center_obj->GetPositionX(), center_obj->GetPositionY()));
    Cell cell(p);
    if (dont_load)
        cell.SetNoCreate();

    TypeContainerVisitor<T, GridTypeMapContainer> gnotifier(visitor);
    cell.Visit(p, gnotifier, *center_obj->GetMap(), *center_obj, radius);
}

template<class T>
inline void Cell::VisitWorldObjects(WorldObject const* center_obj, T& visitor, float radius, bool dont_load /*= true*/)
{
    CellCoord p(Trinity::ComputeCellCoord(center_obj->GetPositionX(), center_obj->GetPositionY()));
    Cell cell(p);
    if (dont_load)
        cell.SetNoCreate();

    TypeContainerVisitor<T, WorldTypeMapContainer> wnotifier(visitor);
    cell.Visit(p, wnotifier, *center_obj->GetMap(), *center_obj, radius);
}

template<class T>
inline void Cell::VisitAllObjects(WorldObject const* center_obj, T& visitor, float radius, bool dont_load /*= true*/)
{
    CellCoord p(Trinity::ComputeCellCoord(center_obj->GetPositionX(), center_obj->GetPositionY()));
    Cell cell(p);
    if (dont_load)
        cell.SetNoCreate();

    TypeContainerVisitor<T, WorldTypeMapContainer> wnotifier(visitor);
    cell.Visit(p, wnotifier, *center_obj->GetMap(), *center_obj, radius);
    TypeContainerVisitor<T, GridTypeMapContainer> gnotifier(visitor);
    cell.Visit(p, gnotifier, *center_obj->GetMap(), *center_obj, radius);
}

template<class T>
inline void Cell::VisitGridObjects(float x, float y, Map* map, T& visitor, float radius, bool dont_load /*= true*/)
{
    CellCoord p(Trinity::ComputeCellCoord(x, y));
    Cell cell(p);
    if (dont_load)
        cell.SetNoCreate();

    TypeContainerVisitor<T, GridTypeMapContainer> gnotifier(visitor);
    cell.Visit(p, gnotifier, *map, x, y, radius);
}

template<class T>
inline void Cell::VisitWorldObjects(float x, float y, Map* map, T& visitor, float radius, bool dont_load /*= true*/)
{
    CellCoord p(Trinity::ComputeCellCoord(x, y));
    Cell cell(p);
    if (dont_load)
        cell.SetNoCreate();

    TypeContainerVisitor<T, WorldTypeMapContainer> wnotifier(visitor);
    cell.Visit(p, wnotifier, *map, x, y, radius);
}

template<class T>
inline void Cell::VisitAllObjects(float x, float y, Map* map, T& visitor, float radius, bool dont_load /*= true*/)
{
    CellCoord p(Trinity::ComputeCellCoord(x, y));
    Cell cell(p);
    if (dont_load)
        cell.SetNoCreate();

    TypeContainerVisitor<T, WorldTypeMapContainer> wnotifier(visitor);
    cell.Visit(p, wnotifier, *map, x, y, radius);
    TypeContainerVisitor<T, GridTypeMapContainer> gnotifier(visitor);
    cell.Visit(p, gnotifier, *map, x, y, radius);
}

#endif
