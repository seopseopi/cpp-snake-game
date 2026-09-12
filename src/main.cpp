// 스네이크 게임의 메인 루프, 초기화 및 게임의 전반적인 흐름을 제어하는 진입점
#include <ncurses.h>
#include <cstdlib>
#include <ctime>
#include <chrono>
#include <memory>
#include <algorithm>
#include <string>

#include "Map.hpp"
#include "Gate.hpp"
#include "Item.hpp"
#include "ItemSnake.hpp"
#include "Snake.hpp"
#include "ScoreBoard.hpp"
#include "RedWallProjectile.hpp"
#include "Zone.hpp"

// 스테이지별 맵 파일 경로 (Stage 1~5)
namespace {

const char* kStageMaps[] = {
    nullptr,          // 인덱스 0은 사용하지 않음
    "data/map1.txt",  // Stage 1
    "data/map2.txt",  // Stage 2
    "data/map3.txt",  // Stage 3
    "data/map4.txt",  // Stage 4
    "data/map5.txt",  // Stage 5
};

} // namespace

int main(int argc, char* argv[]) {
    // 난수 생성기 초기화 (프로그램 시작 시 1번만 실행)
    std::srand(static_cast<unsigned>(std::time(nullptr)));

    // ncurses 초기화
    initscr();
    start_color();
    noecho();
    curs_set(0);
    keypad(stdscr, TRUE);

    // 디버그 스테이지 설정
    int startStage = 1;
    if (argc >= 2) {
        startStage = std::atoi(argv[1]);
        if (startStage < 1) startStage = 1;
        if (startStage > 5) startStage = 5;
    }

    // 시간 측정은 "스테이지별"로 한다.
    // - stageStart: 현재 스테이지가 시작된 실제 시각
    // - 매 프레임 (now - stageStart)를 초로 환산해 ScoreBoard에 보낸다
    // - 스테이지 클리어 → Enter 대기 → 다음 스테이지 진입 시 stageStart를 다시 now()로 리셋
    //   (Enter 누르고 있던 시간이 다음 스테이지 시간에 더해지지 않도록 하기 위함)
    using clock = std::chrono::steady_clock;
    auto stageStart = clock::now();

    // 시작 스테이지 맵 로드 및 게임 객체 생성
    Map gameMap(kStageMaps[startStage]);
    auto snake = std::make_unique<ItemSnake>(gameMap);

    GateManager gateManager(gameMap);
    gateManager.spawnGates();

    ItemManager itemManager(gameMap);
    ZoneManager zoneManager(gameMap);  // 슬로우/패스트 존 (시간 제한 스폰)
    RedWallProjectileManager redWall(gameMap);

    // Score Board 초기화
    ScoreBoard scoreBoard;
    scoreBoard.setStage(startStage);
    scoreBoard.resetForNewStage(snake->getLength());

    // Score Board 시작 열 위치: 맵 가로 크기(셀 * 2칸) + 여백 2칸
    const int scoreCol = gameMap.getWidth() * 2 + 2;

    // ── 하단 알림 (맵 아래 한 줄: 존·아이템·미스터리·빨간 블록·속도 변화) ──
    // 맵 draw()가 COLOR_PAIR 1~18을 매 프레임 재정의하므로, 알림은 20번대만 사용한다.
    std::string statusMsg;        // 표시할 문구
    int statusMsgTicks = 0;       // 남은 표시 틱 수 (0이면 안 보임)

    // 메시지 종류별 색을 분리 — 발표할 때 "이 줄이 무슨 의미인지" 설명하기 쉽도록
    enum StatusStyle {
        STATUS_DEFAULT,  // 흰  : 기본
        STATUS_WARN,     // 노랑: 속도 변화
        STATUS_GOOD,     // 초록: 성장·패스트 존
        STATUS_BAD,      // 빨강: 독·빨간 블록
        STATUS_INFO      // 시안: 슬로우·무적·미스터리
    };
    StatusStyle statusStyle = STATUS_DEFAULT;

    // 메시지 1개를 띄우는 헬퍼. 새 메시지가 오면 기존 메시지를 덮어쓴다.
    auto showStatus = [&](const char* msg, int ticks = 20, StatusStyle style = STATUS_DEFAULT) {
        statusMsg = msg;
        statusMsgTicks = ticks;
        statusStyle = style;
    };

    // 스타일 → ncurses 색 페어(20~24) 매핑
    auto styleToColorPair = [](StatusStyle style) {
        switch (style) {
            case STATUS_WARN: return 21;
            case STATUS_GOOD: return 22;
            case STATUS_BAD:  return 23;
            case STATUS_INFO: return 24;
            default:          return 20;
        }
    };

    // 화면 전체를 다시 그리는 람다 함수 (맵 + Score Board + Gate 교체 카운트다운)
    auto paintUi = [&]() {
        clear();
        gameMap.draw(redWall.mapDrawBlinkPhase());
        redWall.drawOverlay();
        const int gateCd = gateManager.respawnCountdown();
        scoreBoard.draw(scoreCol, 0, gateCd);
        
        // 하단 알림 출력 (검정 배경 + 글자색만 사용)
        // - 맵 draw()가 7번을 초록/초록(Growth)로 쓰기 때문에 겹치지 않게 20번대만 사용한다
        if (!statusMsg.empty()) {
            const bool color = has_colors();
            const int statusPair = styleToColorPair(statusStyle);

            if (color) {
                // 메시지에 쓸 색 페어들을 (안전하게 매 프레임) 등록
                init_pair(20, COLOR_WHITE,  COLOR_BLACK);
                init_pair(21, COLOR_YELLOW, COLOR_BLACK);
                init_pair(22, COLOR_GREEN,  COLOR_BLACK);
                init_pair(23, COLOR_RED,    COLOR_BLACK);
                init_pair(24, COLOR_CYAN,   COLOR_BLACK);

                attron(COLOR_PAIR(statusPair) | A_BOLD);
            } else {
                attron(A_BOLD);
            }

            // 맵 바로 아래 한 줄에 메시지를 찍는다
            mvprintw(gameMap.getHeight() + 1, 0, "%s", statusMsg.c_str());

            if (color) {
                attroff(COLOR_PAIR(statusPair) | A_BOLD);
            } else {
                attroff(A_BOLD);
            }
        }

        // 무적 시간 표시 (머리 위에 남은 초 출력)
        const int invTicks = snake->getInvincibleTicks();
        if (invTicks > 0) {
            const std::pair<int, int> head = snake->getHeadPos();
            // 머리 위에 한 줄 공간이 있을 때만 표시 (맵 상단 경계 제외)
            if (head.first > 0) {
                // 10틱 = 1초 → 올림 처리해서 자연수로 보이게 (예: 9틱 → 1초)
                const int remainSec = (invTicks + 9) / 10;
                attron(COLOR_PAIR(11) | A_BOLD);
                mvprintw(head.first - 1, head.second * 2, "%d", remainSec);
                attroff(COLOR_PAIR(11) | A_BOLD);
            }
        }

        // Gate 교체 직전 카운트다운(1~3초)을 게이트 타일 위에 큰 숫자로 표시
        // (자색/자색 페어는 숫자가 안 보이므로 노란 배경 페어로 덮어 씀)
        if (gateCd > 0) {
            const GatePos g1 = gateManager.getGate1();
            const GatePos g2 = gateManager.getGate2();
            const int drawX1 = g1.x * 2;   // 가로는 2칸 폭이라 *2
            const int drawX2 = g2.x * 2;
            const chtype num = static_cast<chtype>('0' + gateCd);  // '1','2','3'
            if (has_colors()) {
                attron(COLOR_PAIR(12) | A_BOLD);
            } else {
                attron(A_STANDOUT | A_BOLD);
            }
            mvaddch(g1.y, drawX1, num);
            mvaddch(g1.y, drawX1 + 1, num);
            mvaddch(g2.y, drawX2, num);
            mvaddch(g2.y, drawX2 + 1, num);
            if (has_colors()) {
                attroff(COLOR_PAIR(12) | A_BOLD);
            } else {
                attroff(A_STANDOUT | A_BOLD);
            }
        }
        refresh();
    };

    // 초기 화면 출력
    paintUi();

    // 100ms마다 getch()가 반환하도록 비차단 모드 설정
    timeout(100);

    int ch = 0;
    int tick = 0;
    bool gameOver = false;
    int lastGateCd = -1;
    int lastPaintedSecond = -1;
    int currentTimeout = 100;

    // 아이템 효과에 의한 속도 조절
    int speedModifierFromItems = 0;
    int growthCountForSpeed = 0;
    int poisonCountForSpeed = 0;
    int lastHeadZone = ZONE_NONE;

    // ── 메인 게임 루프 ──
    while (true) {
        ch = getch();
        if (ch == 'q' || ch == 'Q') break;  // Q키로 게임 종료

        if (statusMsgTicks > 0) statusMsgTicks--;
        else statusMsg.clear();

        // 현재 스테이지가 시작된 뒤로 흐른 초를 계산해 Score Board에 반영
        // (스테이지가 바뀌면 stageStart가 now()로 리셋되어 자동으로 00:00부터 다시 시작)
        const int stageElapsedSec = static_cast<int>(
            std::chrono::duration_cast<std::chrono::seconds>(clock::now() - stageStart).count());
        scoreBoard.setElapsedSeconds(stageElapsedSec);

        // ── 속도 계산 ──
        // 다음 프레임까지 기다릴 시간(ms)을 구한다. 값이 작을수록 게임이 빨라진다.
        // 최종 = (스테이지 기본 속도) - (아이템 보정) + (존 보정)
        //
        // 이번 프레임 한정으로만 쓰는 값들이라 전부 const
        const int baseTimeout     = 100 - (stageElapsedSec / 15) * 10;      // 15초마다 10ms씩 빨라짐
        const std::pair<int, int> headPos = snake->getHeadPos();            // 뱀 머리 좌표
        const int headZone        = gameMap.getZone(headPos.first, headPos.second);             // 머리가 밟은 존
        const int zoneTimeoutDelta = gameMap.getZoneTimeoutDelta(headPos.first, headPos.second); // 존이 주는 가감속(±ms)

        // 너무 빠르면 손이 못 따라가고, 너무 느리면 답답하니까 30~180ms로 클램프
        int targetTimeout = baseTimeout - speedModifierFromItems + zoneTimeoutDelta;
        if (targetTimeout < 30)  targetTimeout = 30;
        if (targetTimeout > 180) targetTimeout = 180;

        // 머리가 새로운 존에 들어간 순간(=직전 존과 다를 때)에만 메시지를 띄운다
        if (headZone != lastHeadZone) {
            if (headZone == ZONE_SLOW)      showStatus("Slow Zone", 15, STATUS_INFO);
            else if (headZone == ZONE_FAST) showStatus("Fast Zone", 15, STATUS_GOOD);
            lastHeadZone = headZone;
        }

        if (targetTimeout < currentTimeout) {
            currentTimeout = targetTimeout;
            if (statusMsgTicks <= 0)
                showStatus("Speed Up!", 20, STATUS_WARN);
        } else if (targetTimeout > currentTimeout) {
            currentTimeout = targetTimeout;
            if (statusMsgTicks <= 0)
                showStatus("Speed Down!", 20, STATUS_WARN);
        }
        timeout(currentTimeout);

        // 무적 시간 감소
        snake->tickInvincible();

        bool redraw = false;

        // 키 입력이 있으면 뱀 방향 전환 (반대 방향 입력 시 즉사)
        if (ch != ERR) {
            if (!snake->updateDirection(ch)) {
                gameOver = true;
                break;
            }
        }

        // 아이템 매니저 업데이트 (수명 체크, 소멸/재생성)
        if (itemManager.update(gameMap)) {
            redraw = true;
        }

        // 속도 존 업데이트 (일정 시간 후 사라지고 다른 위치에 재생성)
        if (zoneManager.update(gameMap)) {
            redraw = true;
        }

        // 게이트 매니저 업데이트 (20초마다 위치 재생성)
        if (gateManager.update()) {
            redraw = true;
        }

        // 3틱(300ms)마다 뱀을 1칸 이동시킨다.
        bool snakeStep = false;
        tick++;
        if (tick % 3 == 0) {
            snakeStep = true;
            // Gate 통과 여부를 감지하기 위해 이동 전 상태를 저장
            // (이 이동 한 번 동안 안 바뀌는 값들이라 const)
            const bool wasPassage = gateManager.isPassageActive();
            const int moveResult = snake->move(gameMap, gateManager);

            // 이동 결과가 사망이면 게임 오버
            if (moveResult == SNAKE_MOVE_DEAD) {
                gameOver = true;
                break;
            }
            gateManager.afterSnakeMove();

            // 이동 결과에 따라 Score Board·하단 알림 반영
            if (moveResult == SNAKE_MOVE_GROWTH) {
                scoreBoard.addGrowth();
                growthCountForSpeed++;
                bool speedChanged = false;
                if (growthCountForSpeed >= 4) {
                    growthCountForSpeed = 0;
                    speedModifierFromItems += 10;
                    speedChanged = true;
                }
                if (speedChanged) showStatus("Growth! (+1) & Speed Up!", 30, STATUS_WARN);
                else showStatus("Growth! Length +1", 20, STATUS_GOOD);
            } else if (moveResult == SNAKE_MOVE_MYSTERY_GROWTH) {
                scoreBoard.addGrowth();
                growthCountForSpeed++;
                bool speedChanged = false;
                if (growthCountForSpeed >= 4) {
                    growthCountForSpeed = 0;
                    speedModifierFromItems += 10;
                    speedChanged = true;
                }
                if (speedChanged) showStatus("Mystery Box: Growth! (+1) & Speed Up!", 30, STATUS_WARN);
                else showStatus("Mystery Box: Growth! (+1)", 22, STATUS_INFO);
            } else if (moveResult == SNAKE_MOVE_POISON) {
                scoreBoard.addPoison();
                poisonCountForSpeed++;
                bool speedChanged = false;
                if (poisonCountForSpeed >= 3) {
                    poisonCountForSpeed = 0;
                    speedModifierFromItems -= 10;
                    speedChanged = true;
                }
                if (speedChanged) showStatus("Poison! (-1) & Speed Down!", 30, STATUS_WARN);
                else showStatus("Poison! Length -1", 20, STATUS_BAD);
            } else if (moveResult == SNAKE_MOVE_MYSTERY_POISON) {
                scoreBoard.addPoison();
                poisonCountForSpeed++;
                bool speedChanged = false;
                if (poisonCountForSpeed >= 3) {
                    poisonCountForSpeed = 0;
                    speedModifierFromItems -= 10;
                    speedChanged = true;
                }
                if (speedChanged) showStatus("Mystery Box: Poison! (-1) & Speed Down!", 30, STATUS_WARN);
                else showStatus("Mystery Box: Poison! (-1)", 22, STATUS_INFO);
            } else if (moveResult == SNAKE_MOVE_INVINCIBLE) {
                showStatus("Invincible! (5s)", 22, STATUS_INFO);
            } else if (moveResult == SNAKE_MOVE_MYSTERY_INVINCIBLE) {
                showStatus("Mystery Box: Invincible! (5s)", 22, STATUS_INFO);
            }
            // 이동 전에는 Gate 통과 중이 아니었는데, 이동 후 통과 중이면 Gate 사용으로 판단
            if (!wasPassage && gateManager.isPassageActive()) {
                scoreBoard.addGate();     // Gate 사용 횟수 증가
            }

            // 뱀 길이를 Score Board에 반영 (최대 길이도 자동 갱신)
            scoreBoard.updateLength(snake->getLength());
            redraw = true;

            // ── 미션 달성 체크 ──
            if (scoreBoard.isMissionClear()) {
                const bool isFinal = (scoreBoard.getStage() >= 5);

                // 이 스테이지의 최종 클리어 시간을 ScoreBoard에 한 번 더 박아넣는다.
                // (이렇게 해두면 Enter 대기 동안 표시되는 시간이 흘러가지 않는다)
                scoreBoard.setElapsedSeconds(stageElapsedSec);
                // 클리어 시간을 랭킹 기록에 저장한다.
                scoreBoard.finishStageTimer();

                // 화면에 맵 + Score Board + 클리어 팝업을 출력
                clear();
                gameMap.draw(redWall.mapDrawBlinkPhase());
                redWall.drawOverlay();
                scoreBoard.draw(scoreCol, 0);
                scoreBoard.drawClearPopup(gameMap.getWidth(), gameMap.getHeight(), isFinal);
                refresh();

                // Enter 키 입력 대기
                timeout(-1);
                while (true) {
                    const int endCh = getch();
                    if (endCh == '\n' || endCh == '\r' || endCh == KEY_ENTER) break;
                }

                // 마지막 스테이지였으면 게임 종료
                if (isFinal) {
                    endwin();
                    return 0;
                }

                // 다음 스테이지로 전환
                const int nextStage = scoreBoard.getStage() + 1;

                // 기존 Gate 제거 후 새 맵 로드
                gateManager.removeGates();
                gameMap.resetFromFile(kStageMaps[nextStage]);

                // 아이템, 뱀, Gate를 새 맵 기준으로 재생성
                itemManager.resetForNewMap(gameMap);
                zoneManager.resetForNewMap(gameMap);
                snake = std::make_unique<ItemSnake>(gameMap);
                gateManager.spawnGates();
                redWall.resetForNewMap(gameMap);

                // Score Board를 다음 스테이지 목표로 갱신
                scoreBoard.setStage(nextStage);
                scoreBoard.resetForNewStage(snake->getLength());
                tick = 0;
                redraw = true;

                // 시간 측정 리셋: 다음 스테이지는 00:00부터 다시 시작
                // (Enter 누르고 있던 시간은 자연스럽게 무시된다)
                stageStart = clock::now();
                scoreBoard.setElapsedSeconds(0);
                lastPaintedSecond = -1;  // 화면도 강제로 다시 그리도록

                // 속도 관련 상태도 초기화
                speedModifierFromItems = 0;
                growthCountForSpeed = 0;
                poisonCountForSpeed = 0;
                currentTimeout = 100;
                lastHeadZone = ZONE_NONE;

                // 비차단 모드 복원
                timeout(currentTimeout);
            }
        }

        const auto redWallResult = redWall.update(gameMap, *snake, snakeStep);
        if (redWallResult.gameOver) {
            gameOver = true;
            break;
        }
        if (redWallResult.needsRedraw) {
            redraw = true;
            scoreBoard.updateLength(snake->getLength());
        }
        if (redWallResult.snakeHitRedWall) {
            showStatus("Red Block Hit! Length -3", 25, STATUS_BAD);
            redraw = true;
        }

        // Gate 교체 카운트(1~3)가 바뀌면 반드시 다시 그림 (초 단위 게임 타이머와 어긋나도 표시 유지)
        const int gateCd = gateManager.respawnCountdown();
        if (gateCd != lastGateCd) {
            redraw = true;
        }

        // 맵/뱀 변화 또는 시간 변화가 있을 때만 화면을 다시 그린다.
        const bool timeTick = (stageElapsedSec != lastPaintedSecond);
        if (redraw || timeTick) {
            lastPaintedSecond = stageElapsedSec;
            paintUi();
        }

        lastGateCd = gateCd;
    }

    // ── 게임 오버 처리 (빨간색 팝업) ──
    if (gameOver) {
        // 사망한 스테이지를 F로 기록한다.
        scoreBoard.markStageFailed();
        clear();
        gameMap.draw(redWall.mapDrawBlinkPhase());
        redWall.drawOverlay();
        scoreBoard.draw(scoreCol, 0);
        scoreBoard.drawGameOverPopup(gameMap.getWidth(), gameMap.getHeight());
        refresh();

        // Enter 키 입력 대기 후 종료
        timeout(-1);
        while (true) {
            const int endCh = getch();
            if (endCh == '\n' || endCh == '\r' || endCh == KEY_ENTER) break;
        }
    }

    // ncurses 종료
    endwin();
    return 0;
}
