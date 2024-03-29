#include "gameObj.h"
#include "engine.h"
#include "collider.h"
#include "mvmt.h"
#include <algorithm>
#include <string>
#include "ncurses.h"

#include <fstream> 
#include <iostream>

GameObj::GameObj(size_t r, size_t c, int height, Engine& e, char ch): r{r}, c{c}, height{height}, 
  e{e}, mvmtY{0}, mvmtX{0} , wait{1}, period{1}, inPlay{true} {
    bmp[std::make_pair(0, 0)] = ch;
}
GameObj::GameObj(size_t r, size_t c, int height, Engine& e, size_t w, size_t l, char ch):
  r{r}, c{c}, height{height}, e{e}, mvmtY{0}, mvmtX{0}, wait{1}, period{1}, inPlay{true} {
    for (int x=0; x<w; ++x) {
        for (int y=0; y<l; ++y) {
            bmp[std::make_pair(y,x)] = ch;
        }
    }
}
GameObj::GameObj(size_t r, size_t c, int height, Engine& e, std::map<std::pair<int,int>, char> bitmap):
  r{r}, c{c}, height{height}, e{e}, mvmtY{0}, mvmtX{0}, wait{1}, period{1}, bmp{bitmap}, inPlay{true} {}


void GameObj::computeCells() {
    occupiedCells.reserve(bmp.size());
    occupiedCells.clear();

    int minr, maxr, minc, maxc;
    minr = minc = 10000; // arbitrarily large number
    maxr = maxc = 0; 
    for (auto& it : bmp) {
        int y = it.first.first;
        int x = it.first.second; 

        minc = std::min(minc, y);
        maxc = std::max(maxc, y);
        minr = std::min(minr, x);
        maxr = std::max(maxr, x);
        
        occupiedCells.push_back(&(e.getGrid()[y + r][x + c]));
    }

    if (minr || minc) {
        reindexBmp(minc, minr);
    }

    // new rectangular dimensions
    length = maxc - minc;
    width = maxr - minr;
}

void GameObj::reindexBmp(int dY, int dX) {
    std::map<std::pair<int,int>, char> newBmp;
    for (auto& it : bmp) {
        int y = it.first.first;
        int x = it.first.second; 
        newBmp[std::make_pair(y - dY, x - dX)] = it.second;
    }
    std::swap(newBmp, bmp);

    r += dY; 
    c += dX;
}


char GameObj::charAt(size_t row, size_t col) const {
    // try {
        return bmp.at(std::make_pair(row-r, col-c));
    // }
    // catch (...) {
        // e.updateViews(0, std::to_string(row) + " " + std::to_string(col));
        // e.updateViews(1, std::to_string(r) + " " + std::to_string(c));
        // e.updateViews(2, std::to_string(row-r) + " " + std::to_string(col-c));
        // return ' ';
    // }
    // return 
}

void GameObj::addCollider(std::unique_ptr<Collider> c){
    colliders.emplace_back(std::move(c));
    // return (colliders.end())->get();
}

void GameObj::addMvmt(std::unique_ptr<Mvmt> m) {
    movements.emplace_back(std::move(m));
}
void GameObj::clearMvmts() { // likely results in dangling ptrs
    movements.clear();
}




void GameObj::destroyCharAt(Cell& posn) {
    posn.rmObj(this);
    bmp.erase(std::make_pair(posn.r - r, posn.c - c));
    auto it = std::find(occupiedCells.begin(), occupiedCells.end(), &posn);
    occupiedCells.erase(it);
}

void GameObj::selfDestruct() {
    for (auto cell : occupiedCells) {
        cell->rmObj(this);
    }
    bmp.clear();
    height = -1;
    e.ignore(this);
    inPlay = false;
}



void GameObj::runMvmts() {
    for (auto& it : movements) {
        it->affectMvmt();
    }
}



void GameObj::mvmtCollide() {
    if (!colliders.empty()) {
        for (auto &it : colliders) { // run all my colliders
            it->runCollision(); // mvmtX, mvmtY mutated if necessary
        }
    }
}
void GameObj::dmgCollide(Cell& cell) {
    for (auto& it : colliders) {
        it->collide(cell);
    }
}

// collides with all cells of same height at Cell cell
// returns if any collision has happened
bool GameObj::collideAt(Cell& cell) {
    std::vector<GameObj*> collidingObjs = cell.getObjsAtHeight(height);
    bool collision = !collidingObjs.empty();
    if (collision) {
        for (auto it : collidingObjs) {
            it->mvmtCollide();
            it->dmgCollide(cell);
        }
        // then call my own collisions
        mvmtCollide();
        // dmgCollide(cell); // THIS DOES'NT DO ANYTHING B/C R,C AREN'T UPDATED. DMGCOLLIDING MYSELF ON A NONEXISTENT BMP KEY
    }
    return collision; // return if 'this' collided
} 

void GameObj::resetColliders() {
    for (auto& collider : colliders) {
        collider->resetCollision();
    }
}

// attempt to move by 'this's defined motion vector (mvmtX, mvmtY)
void GameObj::move() {

    if (wait) { // check if there are ticks to wait until mvmt
        --wait;
        return; 
    } else { wait = period; } // reset ticks, and proceed with move

    runMvmts(); // applies all Mvmt modifiers to 'this'

    computeCollisions(); // move and see if collide. if collide, then act on collisions. 
    // may need to call tryMove to see if resulting mutations on mvmtXY result in further collision

    // collisions have been resolved, lift obj then place it down into destination cells
    moveCoords();

    // TODO: somehow signal a removal if size of occupiedCells == 0
    // this is stand-in
    if (bmp.empty()) {
        height = -1;
        setMvmt(0,0);
        e.createStopCollider(this);
        e.ignore(this);
        inPlay = false;
    }
    
}

void GameObj::computeCollisions() {
    for (auto it : bmp) { // for every cell 'this' currently occupies,
        int y = it.first.first;
        int x = it.first.second; 
        size_t toR = r + y + mvmtY; 
        size_t toC = c + x + mvmtX;

        // TODO: general non-char objs - does it just work? SOMETHING OFF HERE
        if (toR < 0 || toR >= e.rows || toC < 0 || toC >= e.cols) { // negative overflow, but doesn't matter
            mvmtCollide();
            break;
        }

        Cell& toCell = e.getGrid()[toR][toC]; // get its destination  cell
        if (toCell.contains(this)) { continue; } // i am already in destination
        if (collideAt(toCell)) { 
            dmgCollide(e.getGrid()[r+y][x+c]); } // collide into destination cell
        // ^ any pixel destruction and mvmt mutation due to collision happens here, on own currently occupying cell

        // MY R AND C ARE NOT UPDATED, SO I'M NOT ACTUALLY OCCUPYING THE CURCELL. 
    }
    computeCells();
}

// removes 'this' obj from current cells, then moves it into destination 
// cells, defined by 'this's mvmt vector
// precondition: assumes it is SAFE to do so
void GameObj::moveCoords() {
// e.updateViews(0, "("+std::to_string(r)+","+std::to_string(c)+") -> ("+ std::to_string(r+mvmtY) + "," + std::to_string(c+mvmtX) + ")" );
    for (auto c : occupiedCells) {
        c->rmObj(this);
    }
    r += mvmtY; 
    c += mvmtX;
    computeCells(); // mutates occupiedCells
    for (auto c : occupiedCells) {
        c->addObj(this);
    }
}

