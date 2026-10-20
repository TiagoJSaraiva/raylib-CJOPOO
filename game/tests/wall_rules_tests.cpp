#include "floor_rules.h"
#include "door_visual_rules.h"
#include "wall_rules.h"

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

float Visibility(const WallRules::Frame& frame, WallRules::Section section, int x, int y) {
    const auto found = frame.find(WallRules::Key{WallRules::Position{x, y}, section});
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
    Room room = MakeRoom({0, 0}, BiomeType::Cave, TileRect{10, 20, 2, 2});
    const auto frame = WallRules::BuildFrame({WallRules::RoomView{&room, 1.0f}});
    assert(Count(frame, WallRules::Section::Upper) == 2);
    assert(Count(frame, WallRules::Section::Bottom) == 2);
    assert(Count(frame, WallRules::Section::Side) == 6); // Both side runs and the two upper caps.
    assert(Has(frame, WallRules::Section::Upper, 10, 19));
    assert(Has(frame, WallRules::Section::Bottom, 10, 21));
    assert(Has(frame, WallRules::Section::Side, 9, 20));
    assert(Has(frame, WallRules::Section::Side, 12, 21));
    assert(Has(frame, WallRules::Section::Side, 9, 19));
    assert(Has(frame, WallRules::Section::Side, 12, 19));
    assert(Has(frame, WallRules::Section::Side, 9, 21));
    assert(!Has(frame, WallRules::Section::Side, 9, 22));
    assert(!Has(frame, WallRules::Section::Side, 12, 22));
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
    TestOpenNorthCorridorAndSealedDoor();
    TestSouthAndWestCorridors();
    TestEastCorridorAndSharedConnection();
    TestSharedNorthCorridorFade();
    TestDeterministicChoices();
    std::cout << "Wall rules tests passed\n";
}
