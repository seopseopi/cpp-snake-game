// 게임 내 아이템(성장, 독, 무적, 미스터리)의 생성 및 수명 관리 클래스 헤더
#ifndef ITEM_HPP
#define ITEM_HPP

#include "Map.hpp"
#include <vector>
#include <utility>

struct Item {
    int y;
    int x;
    int type; // 5: Growth, 6: Poison, 11: Invincible(무적), 12: Mystery(미스터리)
    int lifetimeTicks; // 사라질 때까지 남은 틱수
};

class ItemManager {
public:
    ItemManager(Map& map);

    // 매 틱 마다 호출되어 수명을 감소시키고 아이템을 생성/재생성
    bool update(Map& map);

    // 스테이지/맵 교체 후 아이템 목록을 비우고 맵에 다시 스폰
    void resetForNewMap(Map& map);

    const std::vector<Item>& getItems() const { return items; }

private:
    std::vector<Item> items;
    const int maxItems = 3;     // 맵에 존재할 수 있는 최대 아이템 수
    const int maxLifetime = 50; // 50 틱, 약 5초

    // 빈 칸을 찾아 생성했으면 true, 공간이 없으면 false
    bool spawnItem(Map& map);
};

#endif
