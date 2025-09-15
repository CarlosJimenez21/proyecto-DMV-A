#include <GL/freeglut.h>
#include <vector>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

int WIN_W = 1000;
int WIN_H = 700;
int GRID_SPACING = 25;
bool showGrid = true;
bool showAxes = true;
bool showCoords = true;

struct Color { unsigned char r,g,b; };
struct Point { int x,y; };

enum ShapeType { SH_LINE_DIRECT, SH_LINE_DDA, SH_CIRCLE, SH_ELLIPSE };

struct Shape {
    ShapeType type;
    Color color;
    int thickness;
    Point p1, p2;
};

std::vector<Shape> shapes;
std::vector<Shape> redo_stack;

ShapeType currentType = SH_LINE_DDA;
Color currentColor = {0,0,0};
int currentThickness = 1;

bool waitingSecond = false;
Point firstPoint;


inline Point screenToWorld(int sx, int sy) {
    Point p;
    p.x = sx - WIN_W/2;
    p.y = (WIN_H/2) - sy;
    return p;
}

inline void worldToScreen(const Point &w, int &sx, int &sy) {
    sx = w.x + WIN_W/2;
    sy = WIN_H/2 - w.y;
}

void plotPixelWorld(int x, int y, const Color &c, int thickness=1) {
    int sx, sy; worldToScreen({x,y}, sx, sy);
    glPointSize((GLfloat)thickness);
    glBegin(GL_POINTS);
      glColor3ub(c.r, c.g, c.b);
      glVertex2i(sx, sy);
    glEnd();
}


void drawLineDirect(const Point &p1, const Point &p2, const Color &c, int thickness) {
    long dx = p2.x - p1.x;
    long dy = p2.y - p1.y;
    if (dx==0) {
        int x = p1.x;
        int y0 = std::min(p1.y,p2.y);
        int y1 = std::max(p1.y,p2.y);
        for (int y=y0;y<=y1;++y) plotPixelWorld(x,y,c,thickness);
        return;
    }
    double m = (double)dy / (double)dx;
    double b = p1.y - m * p1.x;
    if (std::abs(m) <= 1.0) {
        int x0 = std::min(p1.x,p2.x);
        int x1 = std::max(p1.x,p2.x);
        for (int x=x0;x<=x1;++x) {
            int y = (int)std::round(m * x + b);
            plotPixelWorld(x,y,c,thickness);
        }
    } else {
        int y0 = std::min(p1.y,p2.y);
        int y1 = std::max(p1.y,p2.y);
        for (int y=y0;y<=y1;++y) {
            int x = (int)std::round((y - b) / m);
            plotPixelWorld(x,y,c,thickness);
        }
    }
}

void drawLineDDA(const Point &p1, const Point &p2, const Color &c, int thickness) {
    int dx = p2.x - p1.x;
    int dy = p2.y - p1.y;
    int steps = std::max(std::abs(dx), std::abs(dy));
    if (steps==0) { plotPixelWorld(p1.x,p1.y,c,thickness); return; }
    double xinc = (double)dx / steps;
    double yinc = (double)dy / steps;
    double x = p1.x; double y = p1.y;
    for (int i=0;i<=steps;++i) {
        plotPixelWorld((int)std::round(x),(int)std::round(y),c,thickness);
        x += xinc; y += yinc;
    }
}

void drawCircleMidpoint(const Point &center, int radius, const Color &c, int thickness) {
    int x = 0;
    int y = radius;
    int d = 1 - radius;
    auto plot8 = [&](int xc, int yc, int x, int y){
        plotPixelWorld(xc + x, yc + y, c, thickness);
        plotPixelWorld(xc - x, yc + y, c, thickness);
        plotPixelWorld(xc + x, yc - y, c, thickness);
        plotPixelWorld(xc - x, yc - y, c, thickness);
        plotPixelWorld(xc + y, yc + x, c, thickness);
        plotPixelWorld(xc - y, yc + x, c, thickness);
        plotPixelWorld(xc + y, yc - x, c, thickness);
        plotPixelWorld(xc - y, yc - x, c, thickness);
    };
    while (x <= y) {
        plot8(center.x, center.y, x, y);
        if (d < 0) {
            d += 2*x + 3;
        } else {
            d += 2*(x - y) + 5;
            --y;
        }
        ++x;
    }
}


