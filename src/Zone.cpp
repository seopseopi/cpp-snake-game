// 맵 상의 특정 영역(속도 존)의 생성, 유지 시간 관리 및 제거 로직 구현부
#include "Zone.hpp"
#include <algorithm>
#include <cstdlib>

// 게임 시작·스테이지 전환 시 맵에 초기 존 배치
ZoneManager::ZoneManager(Map& map) {
    for (int i = 0; i < maxZones; ++i) {
        if (!spawnZone(map)) break;
    }
}

// 빈 칸(0)이면서 다른 존과 겹치지 않는 NxN 영역인지 검사
// (맵을 읽기만 하므로 const Map&)
bool ZoneManager::canPlacePatch(const Map& map, int y, int x, int size) const {
    if (y < 0 || x < 0 || y + size > map.getHeight() || x + size > map.getWidth())
        return false;
    for (int dy = 0; dy < size; ++dy) {
        for (int dx = 0; dx < size; ++dx) {
            const int cy = y + dy;
            const int cx = x + dx;
            if (map.getCell(cy, cx) != 0)
                return false;
            if (map.getZone(cy, cx) != ZONE_NONE)
                return false;
        }
    }
    return true;
}

// Map의 zoneData 레이어에 슬로우/패스트 표시 (바닥 셀 값 0은 유지)
// ZoneManager 자신의 멤버는 안 바꾸므로 const 멤버 함수
void ZoneManager::applyPatch(Map& map, const ZonePatch& patch) const {
    for (int dy = 0; dy < patch.size; ++dy) {
        for (int dx = 0; dx < patch.size; ++dx) {
            map.setZone(patch.y + dy, patch.x + dx, patch.type);
        }
    }
}

void ZoneManager::removePatch(Map& map, const ZonePatch& patch) const {
    for (int dy = 0; dy < patch.size; ++dy) {
        for (int dx = 0; dx < patch.size; ++dx) {
            map.setZone(patch.y + dy, patch.x + dx, ZONE_NONE);
        }
    }
}

bool ZoneManager::update(Map& map) {
    bool changed = false;

    // 수명이 0이 된 패치는 맵에서 지우고 목록에서 제거
    for (auto it = zones.begin(); it != zones.end(); ) {
        it->lifetimeTicks--;
        if (it->lifetimeTicks <= 0) {
            removePatch(map, *it);
            it = zones.erase(it);
            changed = true;
        } else {
            ++it;
        }
    }

    // 항상 maxZones개를 유지 (아이템 매니저와 같은 방식)
    while (static_cast<int>(zones.size()) < maxZones && spawnZone(map)) {
        changed = true;
    }

    return changed;
}

// 빈 칸에 2x2 패치를 랜덤 위치에 생성 (슬로우/패스트 50%)
bool ZoneManager::spawnZone(Map& map) {
    const int h = map.getHeight();
    const int w = map.getWidth();

    // 패치가 맵을 벗어나지 않도록 가능한 좌표 범위를 미리 계산
    // 예: 맵 25칸, 패치 2칸 → y는 0~23 사이여야 한다 (총 24가지 선택지)
    const int rangeY = std::max(1, h - patchSize + 1);
    const int rangeX = std::max(1, w - patchSize + 1);

    // 최대 120번 시도해서 빈 자리를 찾는다
    for (int attempt = 0; attempt < 120; ++attempt) {
        const int y = std::rand() % rangeY;
        const int x = std::rand() % rangeX;

        if (!canPlacePatch(map, y, x, patchSize))
            continue;  // 자리에 벽·아이템·다른 존이 있으면 다시 시도

        // 슬로우/패스트를 동전 던지듯 50%씩 결정
        const int zoneType = (std::rand() % 2 == 0) ? ZONE_SLOW : ZONE_FAST;

        ZonePatch patch;
        patch.y = y;
        patch.x = x;
        patch.size = patchSize;
        patch.type = zoneType;
        patch.lifetimeTicks = maxLifetime;

        applyPatch(map, patch);
        zones.push_back(patch);
        return true;
    }
    // 120번 시도해도 자리를 못 찾으면 이번 프레임엔 그냥 패스
    return false;
}

void ZoneManager::resetForNewMap(Map& map) {
    for (const ZonePatch& z : zones) {
        removePatch(map, z);
    }
    zones.clear();
    for (int i = 0; i < maxZones; ++i) {
        if (!spawnZone(map)) break;
    }
}
