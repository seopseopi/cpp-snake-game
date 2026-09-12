// 맵에 무작위로 생성되어 뱀의 속도에 영향을 주는 속도 존(슬로우/패스트) 관리 클래스 헤더
#ifndef ZONE_HPP
#define ZONE_HPP

#include "Map.hpp"
#include <vector>

// ─────────────────────────────────────────────────────────────
// ZoneManager: 속도 존(슬로우/패스트)을 아이템처럼 일정 시간만 맵에 배치
// - 맵 파일에 고정하지 않고, 빈 칸에 2x2 패치로 랜덤 스폰
// - 수명이 끝나면 Map::zoneData에서 제거 후 다른 위치에 재생성
// - 뱀 머리가 존 안에 있으면 main.cpp에서 이동 속도(timeout) 조절
// ─────────────────────────────────────────────────────────────

// 존 패치 한 덩어리(좌상단 좌표 + 크기 + 타입 + 남은 틱)
struct ZonePatch {
    int y;              // 패치 시작 행
    int x;              // 패치 시작 열
    int size;           // 한 변 길이 (NxN, 기본 2)
    int type;           // ZONE_SLOW(1) 또는 ZONE_FAST(2)
    int lifetimeTicks;  // 사라지기 전 남은 메인 루프 틱 수
};

class ZoneManager {
public:
    // 생성 시 맵에 maxZones개 패치를 즉시 스폰
    explicit ZoneManager(Map& map);

    // 매 메인 루프(약 100ms)마다 호출: 수명 감소 → 만료 시 제거 → 부족하면 재스폰
    // 맵 화면이 바뀌었으면 true 반환 (main에서 redraw 용)
    bool update(Map& map);

    // 스테이지 전환 시 기존 존을 모두 지우고 새 맵에 다시 스폰
    void resetForNewMap(Map& map);

private:
    std::vector<ZonePatch> zones;  // 현재 활성 존 목록

    // 한 번 초기화하면 게임 내내 안 바뀌는 설정값 → const 멤버
    const int maxZones = 2;       // 맵에 동시에 둘 수 있는 패치 개수
    const int maxLifetime = 55;   // 패치 유지 틱 (약 5.5초, 루프 100ms 기준)
    const int patchSize = 2;      // 패치 한 변 타일 수 (2x2)

    bool spawnZone(Map& map);                                                // 생성 성공 여부 반환
    bool canPlacePatch(const Map& map, int y, int x, int size) const;        // 맵·매니저 둘 다 읽기만
    void applyPatch(Map& map, const ZonePatch& patch) const;                 // 맵만 변경, 매니저 자신은 그대로
    void removePatch(Map& map, const ZonePatch& patch) const;                // 맵만 변경, 매니저 자신은 그대로
};

#endif
