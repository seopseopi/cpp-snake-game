// 파일로부터 맵을 읽어오고 ncurses를 이용해 화면에 그리는 Map 클래스 구현부
#include "Map.hpp"
#include "RedWallProjectile.hpp"
#include <cctype>
#include <fstream>
#include <algorithm>

Map::Map(const std::string& filePath) : height(0), width(0) {
    loadFromFile(filePath);
    validateAndResize();
}

// (5단계 ScoreBoard에서 추가) 스테이지 전환 시 맵 데이터를 초기화하고 새 파일을 다시 로드한다.
void Map::resetFromFile(const std::string& filePath) {
    data.clear();
    height = 0;
    width = 0;
    loadFromFile(filePath);
    validateAndResize();
}

void Map::loadFromFile(const std::string& filePath) {
    std::ifstream file(filePath);
    if (!file.is_open()) return;

    data.clear();
    zoneData.clear();

    std::string line;
    while (std::getline(file, line)) {
        if (line.empty()) continue;
        std::vector<int> row;
        std::vector<int> zoneRow;
        // 라인의 각 문자를 const 참조로 읽기만 한다 (변경 없음)
        for (const char c : line) {
            if (std::isdigit(static_cast<unsigned char>(c))) {
                row.push_back(c - '0');
                zoneRow.push_back(ZONE_NONE);
            }
        }
        if (!row.empty()) {
            data.push_back(row);
            zoneData.push_back(zoneRow);
            width = std::max(width, (int)row.size());
        }
    }
    height = data.size();
}

void Map::validateAndResize() {
    const int targetSize = 25;
    if (data.empty()) return;

    if ((int)data.size() < targetSize)
        data.resize(targetSize, std::vector<int>(width, 9));
    if (zoneData.size() < data.size())
        zoneData.resize(data.size(), std::vector<int>(width, ZONE_NONE));

    for (int i = 0; i < (int)data.size(); ++i) {
        if ((int)data[i].size() < targetSize)
            data[i].resize(targetSize, 9);
        if (i >= (int)zoneData.size())
            zoneData.emplace_back(targetSize, ZONE_NONE);
        else if ((int)zoneData[i].size() < targetSize)
            zoneData[i].resize(targetSize, ZONE_NONE);
    }

    height = static_cast<int>(data.size());
    width = static_cast<int>(data[0].size());
}

void Map::draw(int boundaryRedBlinkPhase) const {
    // 기본 색상 설정
    start_color();
    // Gate용 자색을 기본값보다 약간 진하게 (터미널이 init_color 지원할 때만)
    if (can_change_color()) {
        init_color(COLOR_MAGENTA, 520, 100, 520); // ncurses 스케일 0–1000
    }
    init_pair(1, COLOR_WHITE, COLOR_BLACK);    // 기본
    init_pair(2, COLOR_BLUE, COLOR_BLUE);      // 벽
    init_pair(3, COLOR_CYAN, COLOR_CYAN);      // 통과 불가능한 벽 (Immune Wall)
    init_pair(4, COLOR_YELLOW, COLOR_YELLOW);  // 머리 (배경을 노란색으로)
    init_pair(5, COLOR_WHITE, COLOR_WHITE);    // 몸통 (배경을 하얀색으로)
    init_pair(6, COLOR_MAGENTA, COLOR_MAGENTA); // 게이트
    init_pair(7, COLOR_GREEN, COLOR_GREEN);    // Growth 아이템
    init_pair(8, COLOR_RED, COLOR_RED);      // Poison 아이템
    init_pair(13, COLOR_BLACK, COLOR_CYAN);   // 무적 아이템 (밝은 하늘색) 실제 화면에서는 빨간 배경에 검정 *
    init_pair(14, COLOR_WHITE, COLOR_BLUE);   // 미스터리 아이템 (파란 배경에 흰 글씨)
    init_pair(17, COLOR_WHITE, COLOR_BLUE);   // 슬로우 존
    init_pair(18, COLOR_BLACK, COLOR_GREEN);  // 패스트 존

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            // 한 칸을 그리는 동안 안 바뀌는 값들이라 const
            const int val = data[y][x];
            const int zone = getZone(y, x);
            const int drawX = x * 2; // 가로 비율을 맞추기 위해 x좌표 2배

            // 빈 바닥(0) 위에 존 색만 덮어 그림 (뱀·아이템 셀은 아래 switch에서 처리)
            if (val == 0 && zone != ZONE_NONE) {
                const int zonePair = (zone == ZONE_SLOW) ? 17 : 18;
                attron(COLOR_PAIR(zonePair));
                mvaddch(y, drawX, '~');
                mvaddch(y, drawX + 1, ' ');
                attroff(COLOR_PAIR(zonePair));
                continue;
            }
            
            switch (val) {
                case 1: 
                    attron(COLOR_PAIR(2));
                    mvaddch(y, drawX, ' '); 
                    mvaddch(y, drawX + 1, ' '); 
                    attroff(COLOR_PAIR(2));
                    break;
                case 2: 
                    attron(COLOR_PAIR(3));
                    mvaddch(y, drawX, ' '); 
                    mvaddch(y, drawX + 1, ' '); 
                    attroff(COLOR_PAIR(3));
                    break;
                case 3: 
                    attron(COLOR_PAIR(4));
                    mvaddch(y, drawX, ' '); 
                    mvaddch(y, drawX + 1, ' '); 
                    attroff(COLOR_PAIR(4));
                    break;
                case 4:
                    attron(COLOR_PAIR(5));
                    mvaddch(y, drawX, ' ');
                    mvaddch(y, drawX + 1, ' ');
                    attroff(COLOR_PAIR(5));
                    break;
                case 5: // Growth 아이템
                    attron(COLOR_PAIR(7));
                    mvaddch(y, drawX, ' ');
                    mvaddch(y, drawX + 1, ' ');
                    attroff(COLOR_PAIR(7));
                    break;
                case 6: // Poison 아이템
                    attron(COLOR_PAIR(8));
                    mvaddch(y, drawX, ' ');
                    mvaddch(y, drawX + 1, ' ');
                    attroff(COLOR_PAIR(8));
                    break;
                case 11: // 무적 아이템
                    attron(COLOR_PAIR(13) | A_REVERSE);
                    mvaddch(y, drawX, '*');
                    mvaddch(y, drawX + 1, ' ');
                    attroff(COLOR_PAIR(13) | A_REVERSE);
                    break;
                case 12: // 미스터리 아이템
                    attron(COLOR_PAIR(14));
                    mvaddch(y, drawX, '?');
                    mvaddch(y, drawX + 1, ' ');
                    attroff(COLOR_PAIR(14));
                    break;
                case 7: // 게이트
                    attron(COLOR_PAIR(6));
                    mvaddch(y, drawX, ' ');
                    mvaddch(y, drawX + 1, ' ');
                    attroff(COLOR_PAIR(6));
                    break;
                case CELL_RED_WALL_CHARGE: // 경계 빨간 벽 경고(충전)
                    if ((boundaryRedBlinkPhase % 2) == 0) {
                        attron(COLOR_PAIR(8));
                        mvaddch(y, drawX, ' ');
                        mvaddch(y, drawX + 1, ' ');
                        attroff(COLOR_PAIR(8));
                    } else {
                        attron(COLOR_PAIR(2));
                        mvaddch(y, drawX, ' ');
                        mvaddch(y, drawX + 1, ' ');
                        attroff(COLOR_PAIR(2));
                    }
                    break;
                default: 
                    mvaddch(y, drawX, ' '); 
                    mvaddch(y, drawX + 1, ' '); 
                    break;
            }
        }
    }
}

void Map::setCell(int y, int x, int value) {
    if (y >= 0 && y < height && x >= 0 && x < width) data[y][x] = value;
}

int Map::getCell(int y, int x) const {
    if (y >= 0 && y < height && x >= 0 && x < width) return data[y][x];
    return -1;
}

// ── 존 레이어 조회·설정 (뱀 이동과 무관하게 zoneData만 변경) ──

int Map::getZone(int y, int x) const {
    if (y >= 0 && y < height && x >= 0 && x < width &&
        y < (int)zoneData.size() && x < (int)zoneData[y].size()) {
        return zoneData[y][x];
    }
    return ZONE_NONE;
}

void Map::setZone(int y, int x, int zoneType) {
    if (y >= 0 && y < height && x >= 0 && x < width) {
        if (y >= (int)zoneData.size())
            zoneData.resize(height, std::vector<int>(width, ZONE_NONE));
        if (x >= (int)zoneData[y].size())
            zoneData[y].resize(width, ZONE_NONE);
        zoneData[y][x] = zoneType;
    }
}

int Map::getZoneTimeoutDelta(int y, int x) const {
    switch (getZone(y, x)) {
        case ZONE_SLOW: return 35;  // 루프 간격 증가 → 이동 느림
        case ZONE_FAST: return -35; // 루프 간격 감소 → 이동 빠름
        default: return 0;
    }
}
