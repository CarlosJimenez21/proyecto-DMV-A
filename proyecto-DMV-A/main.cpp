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
void drawEllipseMidpoint(const Point &center, int rx, int ry, const Color &c, int thickness)
{
    int rx2 = rx*rx;
    int ry2 = ry*ry;
    int x = 0;
    int y = ry;
    long two_rx2 = 2*rx2;
    long two_ry2 = 2*ry2;


    long px = 0;
    long py = two_rx2 * y;


    long p = (long)std::round(ry2 - (rx2 * ry) + (0.25 * rx2));
    while (px < py) {
    plotPixelWorld(center.x + x, center.y + y, c, thickness);
    plotPixelWorld(center.x - x, center.y + y, c, thickness);
    plotPixelWorld(center.x + x, center.y - y, c, thickness);
    plotPixelWorld(center.x - x, center.y - y, c, thickness);


    ++x;
    px += two_ry2;
    if (p < 0) {
    p += ry2 + px;
    } else {
    --y;
    py -= two_rx2;
    p += ry2 + px - py;
    }
}
p = (long)std::round(ry2*(x+0.5)*(x+0.5) + rx2*(y-1)*(y-1) - rx2*ry2);
while (y >= 0) {
    plotPixelWorld(center.x + x, center.y + y, c, thickness);
    plotPixelWorld(center.x - x, center.y + y, c, thickness);
    plotPixelWorld(center.x + x, center.y - y, c, thickness);
    plotPixelWorld(center.x - x, center.y - y, c, thickness);


    --y;
    py -= two_rx2;
    if (p > 0) {
    p += rx2 - py;
    } else {
    ++x;
    px += two_ry2;
    p += rx2 - py + px;
    }
            }
}
void drawGridAndAxes() {
    if (showGrid) {
        glPointSize(1.0f);
    for (int gx = -WIN_W/2; gx <= WIN_W/2; gx += GRID_SPACING) {
        for (int y = -WIN_H/2; y <= WIN_H/2; ++y) {
            Color cg = {220,220,220};
            plotPixelWorld(gx,y,cg,1);
            }
        }
    }
    if (showAxes) {
    Color ca = {0,0,0};
    for (int x = -WIN_W/2; x <= WIN_W/2; ++x) plotPixelWorld(x,0,ca,1);
    for (int y = -WIN_H/2; y <= WIN_H/2; ++y) plotPixelWorld(0,y,ca,1);
    }
}
void redrawScene();


void display() {
    glClear(GL_COLOR_BUFFER_BIT);


    drawGridAndAxes();


    for (const Shape &s : shapes) {
        switch (s.type) {
            case SH_LINE_DIRECT: drawLineDirect(s.p1,s.p2,s.color,s.thickness); break;
            case SH_LINE_DDA: drawLineDDA(s.p1,s.p2,s.color,s.thickness); break;
            case SH_CIRCLE: {
                int dx = s.p2.x - s.p1.x;
                int dy = s.p2.y - s.p1.y;
                int r = (int)std::round(std::sqrt(dx*dx + dy*dy));
                drawCircleMidpoint(s.p1, r, s.color, s.thickness);
                break;
                }
                case SH_ELLIPSE: {
                    int rx = std::abs(s.p2.x - s.p1.x);
                    int ry = std::abs(s.p2.y - s.p1.y);
                    drawEllipseMidpoint(s.p1, rx, ry, s.color, s.thickness);
                    break;
                    }
            }
    }


    glutSwapBuffers();
}
void commitShape(const Shape &s) {
    shapes.push_back(s);
    redo_stack.clear();
}


void mouse(int button, int state, int sx, int sy) {
    if (button == GLUT_LEFT_BUTTON && state == GLUT_DOWN) {
        Point w = screenToWorld(sx, sy);
        if (!waitingSecond) {
            firstPoint = w;
            waitingSecond = true;
        } else {
            Shape s;
            s.type = currentType;
            s.color = currentColor;
            s.thickness = currentThickness;
            s.p1 = firstPoint;
            s.p2 = w;
            commitShape(s);
            waitingSecond = false;
            glutPostRedisplay();
        }
    }
}

void keyboard(unsigned char key, int x, int y) {
    switch (key) {
        case 'g': case 'G': showGrid = !showGrid; break;
        case 'e': case 'E': showAxes = !showAxes; break;
        case 'c': case 'C': shapes.clear(); redo_stack.clear(); break;
        case 's': case 'S': {
            const char *fname = "export.ppm";
            FILE *f = fopen(fname, "wb");
            if (!f) { printf("Failed to write %s\n", fname); break; }
            unsigned char *pixels = (unsigned char*)malloc(WIN_W * WIN_H * 3);
            glReadPixels(0, 0, WIN_W, WIN_H, GL_RGB, GL_UNSIGNED_BYTE, pixels);
            fprintf(f, "P6\n%d %d\n255\n", WIN_W, WIN_H);
            for (int row = WIN_H-1; row >= 0; --row) {
            fwrite(pixels + row * WIN_W * 3, 1, WIN_W*3, f);
            }
            free(pixels);
            fclose(f);
            printf("Exported %s\n", fname);
            break;
        }
        case 'z': case 'Z': {
        if (!shapes.empty()) { redo_stack.push_back(shapes.back()); shapes.pop_back(); }
        break;
        }
        case 'y': case 'Y': {
        if (!redo_stack.empty()) { shapes.push_back(redo_stack.back()); redo_stack.pop_back(); }
        break;
        }
        case 27: exit(0); break;
    }
    glutPostRedisplay();
}

void menu_setType(int id) {
    switch (id) {
        case 1: currentType = SH_LINE_DIRECT; break;
        case 2: currentType = SH_LINE_DDA; break;
        case 3: currentType = SH_CIRCLE; break;
        case 4: currentType = SH_ELLIPSE; break;
    }
}


void menu_setColor(int id) {
    switch (id) {
        case 1: currentColor = {0,0,0}; break;
        case 2: currentColor = {255,0,0}; break;
        case 3: currentColor = {0,255,0}; break;
        case 4: currentColor = {0,0,255}; break;
        case 5: {
            int r,g,b; printf("Enter R G B (0-255): "); if (scanf("%d %d %d", &r,&g,&b)==3) currentColor = {(unsigned char)r,(unsigned char)g,(unsigned char)b};
            break;
        }
    }
}


void menu_setThickness(int id) {
    switch (id) {
        case 1: currentThickness = 1; break;
        case 2: currentThickness = 2; break;
        case 3: currentThickness = 3; break;
        case 4: currentThickness = 5; break;
    }
}


void menu_tools(int id) {
    switch (id) {
        case 1: shapes.clear(); redo_stack.clear(); break;
        case 2: if (!shapes.empty()) { shapes.pop_back(); } break;
        case 3: {
            keyboard('s',0,0); break; }
    }
    glutPostRedisplay();
}


void popupMenu(int value) {
}
