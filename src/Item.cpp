// 아이템의 무작위 생성, 수명 체크 및 획득 처리를 담당하는 ItemManager 클래스 구현부
#include "Item.hpp"
#include <cstdlib>

ItemManager::ItemManager(Map& map) {
    // 초기 아이템들을 생성
    for (int i = 0; i < maxItems; ++i) {
        if (!spawnItem(map)) break;
    }
}

bool ItemManager::update(Map& map) {
    bool mapChanged = false;
    
    // 수명이 다한 아이템을 확인
    for (auto it = items.begin(); it != items.end(); ) {
        // 뱀이 먹어 해당 칸이 다른 값으로 바뀐 아이템은 즉시 목록에서 제거
        if (map.getCell(it->y, it->x) != it->type) {
            it = items.erase(it);
            mapChanged = true;
            continue;
        }

        it->lifetimeTicks--;
        if (it->lifetimeTicks <= 0) {
            // 뱀이 아직 먹지 않아서 맵에 남아있는 경우 맵에서 제거(0으로 변경)
            if (map.getCell(it->y, it->x) == it->type) {
                map.setCell(it->y, it->x, 0);
            }
            it = items.erase(it);
            mapChanged = true;
        } else {
            ++it;
        }
    }

    // 최대 아이템 수를 유지하도록 새로운 아이템을 생성.
    while ((int)items.size() < maxItems && spawnItem(map)) {
        mapChanged = true;
    }

    return mapChanged;
}

bool ItemManager::spawnItem(Map& map) {
    int y, x;
    int attempts = 0;
    while (attempts < 100) {
        y = std::rand() % map.getHeight();
        x = std::rand() % map.getWidth();
        
        // 아이템은 빈 공간(0)에만 생성될 수 있음.
        if (map.getCell(y, x) == 0) {
            const int randVal = std::rand() % 100;
            int type;
            
            // 아이템 출현 비율 조정 (합이 100이 되도록 설정)
            // 예시: Growth 50%, Poison 20%, Mystery 15%, Invincible 15%
            if (randVal < 50) type = 5;            // 0 ~ 49 (50%)
            else if (randVal < 70) type = 6;       // 50 ~ 69 (20%)
            else if (randVal < 85) type = 12;      // 70 ~ 84 (15%)
            else type = 11;                        // 85 ~ 99 (15%)
            
            const Item item = {y, x, type, maxLifetime};
            items.push_back(item);
            map.setCell(y, x, type);
            return true;
        }
        attempts++;
    }
    return false;
}

// 스테이지 전환 시 기존 아이템을 모두 제거하고 새 맵에 다시 생성
void ItemManager::resetForNewMap(Map& map) {
    items.clear();
    for (int i = 0; i < maxItems; ++i) {
        if (!spawnItem(map)) break;
    }
}
