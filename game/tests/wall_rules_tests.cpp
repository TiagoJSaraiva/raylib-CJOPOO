#include "floor_rules.h"
#include "door_visual_rules.h"
#include "wall_rules.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <vector>

namespace {

Room MakeRoom(RoomCoords coords, BiomeType biome, TileRect bounds) {
    RoomLayout layout;
    layout.widthTiles = bounds.width;
    layout.heightTiles = bounds.height;
    layout.tileBounds = bounds;
    return Room(coords, RoomSeedData{RoomType::Normal, biome, 1}, layout);
}

std::size_t Count(const WallRules::Frame& frame, WallRules::Section section) {
    std::size_t count = 0;
    for (const auto& [key, segment] : frame) {
        (void)key;
        if (segment.section == section) ++count;
    }
    return count;
}

bool Has(const WallRules::Frame& frame, WallRules::Section section, int x, int y) {
    return frame.find(WallRules::Key{WallRules::Position{x, y}, section}) != frame.end();
}

bool HasSidePlacement(const WallRules::Frame& frame, int x, int y,
                      WallRules::SidePlacement placement) {
    return frame.find(WallRules::Key{WallRules::Position{x, y}, WallRules::Section::Side,
                                     placement}) != frame.end();
}

std::size_t CountSidePlacement(const WallRules::Frame& frame,
                               WallRules::SidePlacement placement) {
    return static_cast<std::size_t>(std::count_if(frame.begin(), frame.end(),
        [placement](const auto& entry) {
            return entry.first.section == WallRules::Section::Side &&
                   entry.first.sidePlacement == placement;
        }));
}

const WallRules::Module* FindModule(const std::vector<WallRules::Module>& modules,
                                   WallRules::Section section, int x, int y,
                                   WallRules::SidePlacement placement =
                                       WallRules::SidePlacement::Edge) {
    for (const auto& module : modules) {
        if (module.segment.section == section && module.segment.position.x == x &&
            module.segment.position.y == y && module.segment.sidePlacement == placement) {
            return &module;
        }
    }
    return nullptr;
}

std::size_t CountModules(const std::vector<WallRules::Module>& modules,
                         WallRules::Section section) {
    return static_cast<std::size_t>(std::count_if(modules.begin(), modules.end(),
        [section](const WallRules::Module& module) {
            return module.segment.section == section;
        }));
}

float Visibility(const WallRules::Frame& frame, WallRules::Section section, int x, int y) {
    const auto found = frame.find(WallRules::Key{WallRules::Position{x, y}, section});
    assert(found != frame.end());
    return found->second.visibility;
}

float SidePlacementVisibility(const WallRules::Frame& frame, int x, int y,
                              WallRules::SidePlacement placement) {
    const auto found = frame.find(WallRules::Key{WallRules::Position{x, y},
                                                  WallRules::Section::Side, placement});
    assert(found != frame.end());
    return found->second.visibility;
}

void TestSpriteNamingAndWeights() {
    const auto parsed = FloorRules::ParseFilename("8-13.png");
    assert(parsed && parsed->id == 8 && parsed->weight == 13);
    assert(!FloorRules::ParseFilename("0-2.png"));
    assert(!FloorRules::ParseFilename("2-0.png"));
    assert(!FloorRules::ParseFilename("2-3.PNG"));

    std::vector<std::uint64_t> cumulative;
    assert(FloorRules::AddWeight(cumulative, 2));
    assert(FloorRules::AddWeight(cumulative, 5));
    assert(FloorRules::SelectTicket(cumulative, 0) == 0);
    assert(FloorRules::SelectTicket(cumulative, 1) == 0);
    assert(FloorRules::SelectTicket(cumulative, 2) == 1);
    assert(FloorRules::SelectTicket(cumulative, 6) == 1);
    assert(FloorRules::SelectTicket(cumulative, 7) == WallRules::NoSprite);
    assert(!FloorRules::AddWeight(cumulative, UINT64_MAX));
}

void TestRoomPerimeterAndCorners() {
    Room room = MakeRoom({0, 0}, BiomeType::Cave, TileRect{10, 20, 3, 2});
    const auto frame = WallRules::BuildFrame({WallRules::RoomView{&room, 1.0f}});
    assert(Count(frame, WallRules::Section::Upper) == 3);
    assert(Count(frame, WallRules::Section::Bottom) == 3);
    assert(Count(frame, WallRules::Section::Side) == 6); // Side runs plus upper corner caps only.
    assert(Has(frame, WallRules::Section::Upper, 10, 19));
    assert(Has(frame, WallRules::Section::Bottom, 10, 21));
    assert(Has(frame, WallRules::Section::Side, 9, 20));
    assert(Has(frame, WallRules::Section::Side, 13, 21));
    assert(Has(frame, WallRules::Section::Side, 9, 19));
    assert(Has(frame, WallRules::Section::Side, 13, 19));
    assert(Has(frame, WallRules::Section::Side, 9, 21));
    assert(CountSidePlacement(frame, WallRules::SidePlacement::InsetLeft) == 0);
    assert(CountSidePlacement(frame, WallRules::SidePlacement::InsetRight) == 0);
    assert(!Has(frame, WallRules::Section::Side, 9, 22));
    assert(!Has(frame, WallRules::Section::Side, 13, 22));
}

void TestTwoTileWallModules() {
    Room room = MakeRoom({0, 0}, BiomeType::Cave, TileRect{10, 20, 4, 4});
    const auto frame = WallRules::BuildFrame({WallRules::RoomView{&room, 1.0f}});
    const auto modules = WallRules::BuildModules(frame);

    assert(CountModules(modules, WallRules::Section::Upper) == 2);
    assert(CountModules(modules, WallRules::Section::Bottom) == 2);
    assert(CountModules(modules, WallRules::Section::Side) == 6);
    for (const int x : {10, 12}) {
        const auto* upper = FindModule(modules, WallRules::Section::Upper, x, 19);
        const auto* bottom = FindModule(modules, WallRules::Section::Bottom, x, 23);
        assert(upper != nullptr && upper->widthTiles == 2 && upper->heightTiles == 2);
        assert(bottom != nullptr && bottom->widthTiles == 2 && bottom->heightTiles == 2);
    }
    for (const int y : {20, 22}) {
        const auto* left = FindModule(modules, WallRules::Section::Side, 9, y);
        const auto* right = FindModule(modules, WallRules::Section::Side, 14, y);
        assert(left != nullptr && left->widthTiles == 1 && left->heightTiles == 2);
        assert(right != nullptr && right->widthTiles == 1 && right->heightTiles == 2);
    }
    const auto* leftCorner = FindModule(modules, WallRules::Section::Side, 9, 19);
    const auto* rightCorner = FindModule(modules, WallRules::Section::Side, 14, 19);
    assert(leftCorner != nullptr && leftCorner->heightTiles == 2);
    assert(rightCorner != nullptr && rightCorner->heightTiles == 2);

    auto fadingFrame = frame;
    fadingFrame.at(WallRules::Key{WallRules::Position{11, 19},
                                 WallRules::Section::Upper}).visibility = 0.4f;
    const auto fadingModules = WallRules::BuildModules(fadingFrame);
    const auto* fadingUpper = FindModule(fadingModules, WallRules::Section::Upper, 10, 19);
    assert(fadingUpper != nullptr && fadingUpper->widthTiles == 2 &&
           fadingUpper->heightTiles == 2);
    assert(std::abs(fadingUpper->segment.visibility - 0.4f) < 0.001f);

    Room connected = MakeRoom({1, 0}, BiomeType::Dungeon, TileRect{10, 10, 8, 8});
    auto openState = std::make_shared<DoorInstance>();
    openState->open = true;
    const auto addDoor = [&connected, &openState](Direction direction, int offset,
                                                 TileRect corridor) {
        Doorway door;
        door.direction = direction;
        door.offset = offset;
        door.width = 2;
        door.corridorLength = 4;
        door.corridorTiles = corridor;
        door.doorState = openState;
        connected.Layout().doors.push_back(door);
    };
    addDoor(Direction::North, 2, TileRect{12, 6, 2, 4});
    addDoor(Direction::South, 2, TileRect{12, 18, 2, 4});
    addDoor(Direction::East, 2, TileRect{18, 12, 4, 2});
    addDoor(Direction::West, 2, TileRect{6, 12, 4, 2});

    const auto connectedFrame = WallRules::BuildFrame({WallRules::RoomView{&connected, 1.0f}});
    const auto connectedModules = WallRules::BuildModules(connectedFrame);
    for (const auto& module : connectedModules) {
        const auto& segment = module.segment;
        if (segment.section == WallRules::Section::Upper ||
            segment.section == WallRules::Section::Bottom) {
            assert(module.widthTiles == 2 && module.heightTiles == 2);
        } else if (segment.sidePlacement != WallRules::SidePlacement::Edge) {
            assert(module.heightTiles == 1); // Mouth and bottom-corridor caps stay one tile tall.
        } else if (segment.position.y == segment.edgeTile.y) {
            assert(module.heightTiles == 2);
        } else {
            assert(module.heightTiles == 2); // Upper corner caps match the full upper-wall height.
        }
    }
}

void TestBottomCornersInCorridorUnionAndFade() {
    Room room = MakeRoom({0, 0}, BiomeType::Lobby, TileRect{10, 20, 5, 3});
    auto doorState = std::make_shared<DoorInstance>();
    Doorway south;
    south.direction = Direction::South;
    south.offset = 2;
    south.width = 1;
    south.corridorTiles = TileRect{12, 23, 1, 2};
    south.doorState = doorState;
    room.Layout().doors.push_back(south);

    const auto closed = WallRules::BuildFrame({WallRules::RoomView{&room, 1.0f}});
    assert(Has(closed, WallRules::Section::Bottom, 11, 22));
    assert(Has(closed, WallRules::Section::Bottom, 13, 22));
    assert(!HasSidePlacement(closed, 11, 22, WallRules::SidePlacement::InsetRight));
    assert(!HasSidePlacement(closed, 13, 22, WallRules::SidePlacement::InsetLeft));

    doorState->opening = true;
    doorState->fadeProgress = DoorVisualRules::FadeDuration * 0.4f;
    const auto opening = WallRules::BuildFrame({WallRules::RoomView{&room, 1.0f}});
    assert(HasSidePlacement(opening, 11, 22, WallRules::SidePlacement::InsetRight));
    assert(HasSidePlacement(opening, 13, 22, WallRules::SidePlacement::InsetLeft));
    assert(std::abs(SidePlacementVisibility(opening, 11, 22,
                                            WallRules::SidePlacement::InsetRight) - 0.4f) < 0.001f);
    assert(std::abs(SidePlacementVisibility(opening, 13, 22,
                                            WallRules::SidePlacement::InsetLeft) - 0.4f) < 0.001f);
    assert(!HasSidePlacement(opening, 10, 22, WallRules::SidePlacement::InsetLeft));
    assert(!HasSidePlacement(opening, 14, 22, WallRules::SidePlacement::InsetRight));

    doorState->open = true;
    const auto open = WallRules::BuildFrame({WallRules::RoomView{&room, 1.0f}});
    assert(std::abs(SidePlacementVisibility(open, 11, 22,
                                            WallRules::SidePlacement::InsetRight) - 1.0f) < 0.001f);
    assert(std::abs(SidePlacementVisibility(open, 13, 22,
                                            WallRules::SidePlacement::InsetLeft) - 1.0f) < 0.001f);

    Room wideRoom = MakeRoom({1, 0}, BiomeType::Cave, TileRect{20, 20, 5, 3});
    auto wideDoorState = std::make_shared<DoorInstance>();
    wideDoorState->open = true;
    Doorway wideSouth;
    wideSouth.direction = Direction::South;
    wideSouth.offset = 1;
    wideSouth.width = 2;
    wideSouth.corridorTiles = TileRect{21, 23, 2, 2};
    wideSouth.doorState = wideDoorState;
    wideRoom.Layout().doors.push_back(wideSouth);
    const auto wideFrame = WallRules::BuildFrame({WallRules::RoomView{&wideRoom, 1.0f}});
    assert(HasSidePlacement(wideFrame, 20, 22, WallRules::SidePlacement::InsetRight));
    assert(HasSidePlacement(wideFrame, 23, 22, WallRules::SidePlacement::InsetLeft));
    assert(CountSidePlacement(wideFrame, WallRules::SidePlacement::InsetLeft) == 1);
    assert(CountSidePlacement(wideFrame, WallRules::SidePlacement::InsetRight) == 1);
}

void TestSharedSouthCorridorDoesNotDuplicateCaps() {
    Room upper = MakeRoom({0, 0}, BiomeType::Lobby, TileRect{10, 20, 5, 3});
    Room lower = MakeRoom({0, 1}, BiomeType::Cave, TileRect{10, 25, 5, 3});
    auto sharedDoorState = std::make_shared<DoorInstance>();
    sharedDoorState->open = true;

    Doorway south;
    south.direction = Direction::South;
    south.offset = 2;
    south.width = 1;
    south.targetCoords = lower.GetCoords();
    south.corridorTiles = TileRect{12, 23, 1, 2};
    south.doorState = sharedDoorState;
    upper.Layout().doors.push_back(south);

    Doorway north;
    north.direction = Direction::North;
    north.offset = 2;
    north.width = 1;
    north.targetCoords = upper.GetCoords();
    north.corridorTiles = south.corridorTiles;
    north.doorState = sharedDoorState;
    lower.Layout().doors.push_back(north);

    const auto frame = WallRules::BuildFrame({WallRules::RoomView{&upper, 1.0f},
                                               WallRules::RoomView{&lower, 1.0f}});
    assert(CountSidePlacement(frame, WallRules::SidePlacement::InsetRight) == 1);
    assert(CountSidePlacement(frame, WallRules::SidePlacement::InsetLeft) == 1);
    assert(std::abs(SidePlacementVisibility(frame, 11, 22,
                                            WallRules::SidePlacement::InsetRight) - 1.0f) < 0.001f);
}

void TestHorizontalCorridorMouthCapsAndFade() {
    Room eastRoom = MakeRoom({0, 0}, BiomeType::Lobby, TileRect{10, 20, 5, 4});
    auto eastState = std::make_shared<DoorInstance>();
    Doorway eastDoor;
    eastDoor.direction = Direction::East;
    eastDoor.offset = 1;
    eastDoor.width = 2;
    eastDoor.corridorTiles = TileRect{15, 21, 2, 2};
    eastDoor.doorState = eastState;
    eastRoom.Layout().doors.push_back(eastDoor);

    const auto closedEast = WallRules::BuildFrame({WallRules::RoomView{&eastRoom, 1.0f}});
    assert(Has(closedEast, WallRules::Section::Upper, 15, 20));
    assert(Has(closedEast, WallRules::Section::Bottom, 15, 22));
    assert(CountSidePlacement(closedEast, WallRules::SidePlacement::InsetLeft) == 0);

    eastState->opening = true;
    eastState->fadeProgress = DoorVisualRules::FadeDuration * 0.35f;
    const auto openingEast = WallRules::BuildFrame({WallRules::RoomView{&eastRoom, 1.0f}});
    assert(HasSidePlacement(openingEast, 15, 20, WallRules::SidePlacement::InsetLeft));
    assert(HasSidePlacement(openingEast, 15, 22, WallRules::SidePlacement::InsetLeft));
    assert(std::abs(SidePlacementVisibility(openingEast, 15, 20,
                                            WallRules::SidePlacement::InsetLeft) - 0.35f) < 0.001f);
    assert(std::abs(SidePlacementVisibility(openingEast, 15, 22,
                                            WallRules::SidePlacement::InsetLeft) - 0.35f) < 0.001f);
    assert(!HasSidePlacement(openingEast, 16, 20, WallRules::SidePlacement::InsetLeft));
    assert(!HasSidePlacement(openingEast, 15, 20, WallRules::SidePlacement::InsetRight));
    assert(CountSidePlacement(openingEast, WallRules::SidePlacement::InsetLeft) == 2);

    eastState->open = true;
    const auto openEast = WallRules::BuildFrame({WallRules::RoomView{&eastRoom, 1.0f}});
    assert(std::abs(SidePlacementVisibility(openEast, 15, 20,
                                            WallRules::SidePlacement::InsetLeft) - 1.0f) < 0.001f);
    assert(std::abs(SidePlacementVisibility(openEast, 15, 22,
                                            WallRules::SidePlacement::InsetLeft) - 1.0f) < 0.001f);

    Room westRoom = MakeRoom({1, 0}, BiomeType::Cave, TileRect{10, 20, 5, 4});
    auto westState = std::make_shared<DoorInstance>();
    westState->open = true;
    Doorway westDoor;
    westDoor.direction = Direction::West;
    westDoor.offset = 1;
    westDoor.width = 2;
    westDoor.corridorTiles = TileRect{8, 21, 2, 2};
    westDoor.doorState = westState;
    westRoom.Layout().doors.push_back(westDoor);
    const auto openWest = WallRules::BuildFrame({WallRules::RoomView{&westRoom, 1.0f}});
    assert(HasSidePlacement(openWest, 9, 20, WallRules::SidePlacement::InsetRight));
    assert(HasSidePlacement(openWest, 9, 22, WallRules::SidePlacement::InsetRight));
    assert(CountSidePlacement(openWest, WallRules::SidePlacement::InsetRight) == 2);
    assert(!HasSidePlacement(openWest, 8, 20, WallRules::SidePlacement::InsetRight));

    Room west = MakeRoom({2, 0}, BiomeType::Lobby, TileRect{10, 20, 5, 4});
    Room east = MakeRoom({3, 0}, BiomeType::Dungeon, TileRect{19, 20, 5, 4});
    auto sharedState = std::make_shared<DoorInstance>();
    sharedState->opening = true;
    sharedState->fadeProgress = DoorVisualRules::FadeDuration * 0.6f;
    Doorway sharedEast;
    sharedEast.direction = Direction::East;
    sharedEast.offset = 1;
    sharedEast.width = 2;
    sharedEast.targetCoords = east.GetCoords();
    sharedEast.corridorTiles = TileRect{15, 21, 4, 2};
    sharedEast.doorState = sharedState;
    west.Layout().doors.push_back(sharedEast);
    Doorway sharedWest;
    sharedWest.direction = Direction::West;
    sharedWest.offset = 1;
    sharedWest.width = 2;
    sharedWest.targetCoords = west.GetCoords();
    sharedWest.corridorTiles = sharedEast.corridorTiles;
    sharedWest.doorState = sharedState;
    east.Layout().doors.push_back(sharedWest);

    const auto sharedFrame = WallRules::BuildFrame({WallRules::RoomView{&west, 1.0f},
                                                     WallRules::RoomView{&east, 1.0f}});
    assert(CountSidePlacement(sharedFrame, WallRules::SidePlacement::InsetLeft) == 2);
    assert(CountSidePlacement(sharedFrame, WallRules::SidePlacement::InsetRight) == 2);
    for (const int x : {15, 18}) {
        const auto placement = x == 15 ? WallRules::SidePlacement::InsetLeft
                                        : WallRules::SidePlacement::InsetRight;
        assert(HasSidePlacement(sharedFrame, x, 20, placement));
        assert(HasSidePlacement(sharedFrame, x, 22, placement));
        assert(std::abs(SidePlacementVisibility(sharedFrame, x, 20, placement) - 0.6f) < 0.001f);
        assert(std::abs(SidePlacementVisibility(sharedFrame, x, 22, placement) - 0.6f) < 0.001f);
    }

    sharedState->open = true;
    const auto openShared = WallRules::BuildFrame({WallRules::RoomView{&west, 1.0f},
                                                   WallRules::RoomView{&east, 1.0f}});
    assert(CountSidePlacement(openShared, WallRules::SidePlacement::InsetLeft) == 2);
    assert(CountSidePlacement(openShared, WallRules::SidePlacement::InsetRight) == 2);
}

void TestOpenNorthCorridorAndSealedDoor() {
    Room room = MakeRoom({0, 0}, BiomeType::Lobby, TileRect{0, 0, 3, 3});
    auto doorState = std::make_shared<DoorInstance>();
    Doorway north;
    north.direction = Direction::North;
    north.offset = 1;
    north.width = 1;
    north.corridorTiles = TileRect{1, -2, 1, 2};
    north.doorState = doorState;
    room.Layout().doors.push_back(north);

    auto closedFrame = WallRules::BuildFrame({WallRules::RoomView{&room, 1.0f}});
    assert(!Has(closedFrame, WallRules::Section::Upper, 1, -1));
    assert(!Has(closedFrame, WallRules::Section::Upper, 1, -3)); // Corridor endpoint stays open.
    assert(!Has(closedFrame, WallRules::Section::Side, 0, -2));
    assert(!Has(closedFrame, WallRules::Section::Side, 2, -2));
    assert(Has(closedFrame, WallRules::Section::Side, -1, 0)); // Room side remains visible.

    doorState->opening = true;
    doorState->fadeProgress = DoorVisualRules::FadeDuration * 0.4f;
    const auto openingFrame = WallRules::BuildFrame({WallRules::RoomView{&room, 1.0f}});
    assert(std::abs(Visibility(openingFrame, WallRules::Section::Side, 0, -2) - 0.4f) < 0.001f);
    assert(std::abs(Visibility(openingFrame, WallRules::Section::Side, 2, -2) - 0.4f) < 0.001f);
    assert(CountSidePlacement(openingFrame, WallRules::SidePlacement::InsetLeft) == 0);
    assert(CountSidePlacement(openingFrame, WallRules::SidePlacement::InsetRight) == 0);
    assert(std::abs(DoorVisualRules::MaskAlpha(*doorState) - 0.6f) < 0.001f);

    doorState->open = true;
    const auto openFrame = WallRules::BuildFrame({WallRules::RoomView{&room, 1.0f}});
    assert(std::abs(Visibility(openFrame, WallRules::Section::Side, 0, -2) - 1.0f) < 0.001f);
    assert(std::abs(Visibility(openFrame, WallRules::Section::Side, 2, -2) - 1.0f) < 0.001f);

    room.Layout().doors[0].sealed = true;
    auto sealedFrame = WallRules::BuildFrame({WallRules::RoomView{&room, 1.0f}});
    assert(Has(sealedFrame, WallRules::Section::Upper, 1, -1));
    assert(!Has(sealedFrame, WallRules::Section::Bottom, 1, -2));
}

void TestSouthAndWestCorridors() {
    Room southRoom = MakeRoom({0, 0}, BiomeType::Cave, TileRect{0, 0, 3, 3});
    auto southState = std::make_shared<DoorInstance>();
    southState->open = true;
    Doorway south;
    south.direction = Direction::South;
    south.offset = 1;
    south.width = 1;
    south.corridorTiles = TileRect{1, 3, 1, 2};
    south.doorState = southState;
    southRoom.Layout().doors.push_back(south);
    const auto southFrame = WallRules::BuildFrame({WallRules::RoomView{&southRoom, 1.0f}});
    assert(!Has(southFrame, WallRules::Section::Bottom, 1, 4)); // No corridor end cap.
    assert(Has(southFrame, WallRules::Section::Side, 0, 3));
    assert(Has(southFrame, WallRules::Section::Side, 2, 3));
    assert(Visibility(southFrame, WallRules::Section::Side, 0, 3) == 1.0f);

    Room westRoom = MakeRoom({0, 0}, BiomeType::Dungeon, TileRect{0, 0, 3, 3});
    Doorway west;
    west.direction = Direction::West;
    west.offset = 1;
    west.width = 1;
    west.corridorTiles = TileRect{-2, 1, 2, 1};
    westRoom.Layout().doors.push_back(west);
    const auto westFrame = WallRules::BuildFrame({WallRules::RoomView{&westRoom, 1.0f}});
    assert(!Has(westFrame, WallRules::Section::Side, -3, 1)); // No corridor end cap.
    assert(Has(westFrame, WallRules::Section::Upper, -2, 0));
    assert(Has(westFrame, WallRules::Section::Bottom, -1, 1));
}

void TestEastCorridorAndSharedConnection() {
    Room west = MakeRoom({0, 0}, BiomeType::Lobby, TileRect{0, 0, 1, 1});
    Room east = MakeRoom({1, 0}, BiomeType::Cave, TileRect{3, 0, 1, 1});
    Doorway westDoor;
    westDoor.direction = Direction::East;
    westDoor.width = 1;
    westDoor.targetCoords = east.GetCoords();
    westDoor.corridorTiles = TileRect{1, 0, 2, 1};
    Doorway eastDoor;
    eastDoor.direction = Direction::West;
    eastDoor.width = 1;
    eastDoor.targetCoords = west.GetCoords();
    eastDoor.corridorTiles = westDoor.corridorTiles;
    auto sharedDoorState = std::make_shared<DoorInstance>();
    sharedDoorState->open = true;
    westDoor.doorState = sharedDoorState;
    eastDoor.doorState = sharedDoorState;
    west.Layout().doors.push_back(westDoor);
    east.Layout().doors.push_back(eastDoor);

    const auto frame = WallRules::BuildFrame({WallRules::RoomView{&west, 0.25f},
                                               WallRules::RoomView{&east, 0.8f}});
    assert(!Has(frame, WallRules::Section::Side, 3, 0)); // The connected corridor has no internal end cap.
    assert(Has(frame, WallRules::Section::Upper, 1, -1));
    assert(Has(frame, WallRules::Section::Bottom, 2, 0));
    const auto corridorWall = frame.find(WallRules::Key{WallRules::Position{1, -1}, WallRules::Section::Upper});
    assert(corridorWall != frame.end());
    assert(corridorWall->second.biome == BiomeType::Cave);
    assert(corridorWall->second.visibility == 0.8f);
    assert(!Has(frame, WallRules::Section::Side, 1, 0)); // Horizontal corridor keeps its current perimeter.

    const auto westOnly = WallRules::BuildFrame({WallRules::RoomView{&west, 0.25f},
                                                  WallRules::RoomView{&east, 0.0f}});
    const auto hiddenTargetWall = westOnly.find(
        WallRules::Key{WallRules::Position{1, -1}, WallRules::Section::Upper});
    assert(hiddenTargetWall != westOnly.end());
    assert(hiddenTargetWall->second.biome == BiomeType::Cave); // Hidden target still resolves corridor biome.
}

void TestSharedNorthCorridorFade() {
    Room north = MakeRoom({0, 0}, BiomeType::Lobby, TileRect{0, 0, 3, 2});
    Room south = MakeRoom({0, 1}, BiomeType::Cave, TileRect{0, 5, 3, 2});
    auto sharedDoorState = std::make_shared<DoorInstance>();
    sharedDoorState->opening = true;
    sharedDoorState->fadeProgress = DoorVisualRules::FadeDuration * 0.5f;

    Doorway southDoor;
    southDoor.direction = Direction::South;
    southDoor.offset = 1;
    southDoor.width = 1;
    southDoor.targetCoords = south.GetCoords();
    southDoor.corridorTiles = TileRect{1, 2, 1, 3};
    southDoor.doorState = sharedDoorState;
    north.Layout().doors.push_back(southDoor);

    Doorway northDoor;
    northDoor.direction = Direction::North;
    northDoor.offset = 1;
    northDoor.width = 1;
    northDoor.targetCoords = north.GetCoords();
    northDoor.corridorTiles = southDoor.corridorTiles;
    northDoor.doorState = sharedDoorState;
    south.Layout().doors.push_back(northDoor);

    const auto frame = WallRules::BuildFrame({WallRules::RoomView{&north, 0.25f},
                                               WallRules::RoomView{&south, 0.8f}});
    assert(Has(frame, WallRules::Section::Side, 0, 2));
    assert(Has(frame, WallRules::Section::Side, 0, 3));
    assert(Has(frame, WallRules::Section::Side, 0, 4));
    assert(std::abs(Visibility(frame, WallRules::Section::Side, 0, 2) - 0.4f) < 0.001f);
    assert(std::abs(Visibility(frame, WallRules::Section::Side, 2, 4) - 0.4f) < 0.001f);
    assert(!Has(frame, WallRules::Section::Upper, 1, 1));
    assert(!Has(frame, WallRules::Section::Bottom, 1, 4));
}

void TestDeterministicChoices() {
    WallRules::TileMemory memory;
    std::vector<std::uint64_t> cumulative{1, 4, 9};
    memory.Reset(1234);
    const auto first = memory.GetOrCreate({-5, 8}, WallRules::Section::Side,
                                          BiomeType::Dungeon, cumulative);
    memory.Reset(1234);
    const auto repeated = memory.GetOrCreate({-5, 8}, WallRules::Section::Side,
                                             BiomeType::Dungeon, cumulative);
    assert(first.biome == repeated.biome);
    assert(first.sprite == repeated.sprite);
    assert(first.sprite < cumulative.size());
}

} // namespace

int main() {
    TestSpriteNamingAndWeights();
    TestRoomPerimeterAndCorners();
    TestTwoTileWallModules();
    TestBottomCornersInCorridorUnionAndFade();
    TestSharedSouthCorridorDoesNotDuplicateCaps();
    TestHorizontalCorridorMouthCapsAndFade();
    TestOpenNorthCorridorAndSealedDoor();
    TestSouthAndWestCorridors();
    TestEastCorridorAndSharedConnection();
    TestSharedNorthCorridorFade();
    TestDeterministicChoices();
    std::cout << "Wall rules tests passed\n";
}
