#include <GL/freeglut.h>
#include <cmath>
#include <algorithm>
#include <vector>
#include <fstream>
#include <iostream>
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

using namespace std;

// Estructuras de datos
struct Point {
    int x, y;
    Point(int x = 0, int y = 0) : x(x), y(y) {}
};

struct Figure {
    int type; // 0: recta directo, 1: recta DDA, 2: círculo incremental, 3: círculo PM, 4: elipse PM
    vector<Point> points;
    float color[3];
    int thickness;
};

// Variables globales
const int WIDTH = 800;
const int HEIGHT = 600;
const int GRID_SPACING = 20;

vector<Figure> figures;
vector<Figure> undoStack;
Point tempPoints[3];
int pointCount = 0;
int currentTool = 0;
float currentColor[3] = {0.0f, 0.0f, 0.0f};
int currentThickness = 1;
bool showGrid = true;
bool showAxes = true;
bool showCoords = false;
int mouseX = 0, mouseY = 0;

// Prototipos de funciones
void drawPixel(int x, int y);
void drawLineDirect(Point p1, Point p2);
void drawLineDDA(Point p1, Point p2);
void drawCircleIncremental(Point center, int radius);
void drawCircleMidpoint(Point center, int radius);
void drawEllipseMidpoint(Point center, int rx, int ry);
void drawGrid();
void drawAxes();
void displayCoordinates();

// Implementación de algoritmos
void drawPixel(int x, int y) {
    glPointSize(currentThickness);
    glBegin(GL_POINTS);
    glVertex2i(x, y);
    glEnd();
}

void drawLineDirect(Point p1, Point p2) {
    if (p1.x == p2.x) { // Línea vertical
        int y1 = min(p1.y, p2.y);
        int y2 = max(p1.y, p2.y);
        for (int y = y1; y <= y2; y++) {
            drawPixel(p1.x, y);
        }
        return;
    }

    float m = float(p2.y - p1.y) / (p2.x - p1.x);
    float b = p1.y - m * p1.x;

    if (abs(m) <= 1.0f) {
        int x1 = min(p1.x, p2.x);
        int x2 = max(p1.x, p2.x);
        for (int x = x1; x <= x2; x++) {
            int y = round(m * x + b);
            drawPixel(x, y);
        }
    } else {
        int y1 = min(p1.y, p2.y);
        int y2 = max(p1.y, p2.y);
        for (int y = y1; y <= y2; y++) {
            int x = round((y - b) / m);
            drawPixel(x, y);
        }
    }
}

void drawLineDDA(Point p1, Point p2) {
    int dx = p2.x - p1.x;
    int dy = p2.y - p1.y;
    int steps = max(abs(dx), abs(dy));

    if (steps == 0) {
        drawPixel(p1.x, p1.y);
        return;
    }

    float xInc = dx / (float)steps;
    float yInc = dy / (float)steps;

    float x = p1.x;
    float y = p1.y;

    for (int i = 0; i <= steps; i++) {
        drawPixel(round(x), round(y));
        x += xInc;
        y += yInc;
    }
}

void drawCircleIncremental(Point center, int radius) {
    float angle = 0;
    float angleIncrement = 1.0f / radius;

    while (angle < 2 * M_PI) {
        int x = center.x + radius * cos(angle);
        int y = center.y + radius * sin(angle);
        drawPixel(x, y);
        angle += angleIncrement;
    }
}

void drawCircleMidpoint(Point center, int radius) {
    int x = 0;
    int y = radius;
    int d = 1 - radius;

    while (x <= y) {
        // Dibujar los 8 octantes
        drawPixel(center.x + x, center.y + y);
        drawPixel(center.x - x, center.y + y);
        drawPixel(center.x + x, center.y - y);
        drawPixel(center.x - x, center.y - y);
        drawPixel(center.x + y, center.y + x);
        drawPixel(center.x - y, center.y + x);
        drawPixel(center.x + y, center.y - x);
        drawPixel(center.x - y, center.y - x);

        if (d < 0) {
            d += 2 * x + 3;
        } else {
            d += 2 * (x - y) + 5;
            y--;
        }
        x++;
    }
}

void drawEllipseMidpoint(Point center, int rx, int ry) {
    if (rx <= 0 || ry <= 0) return;

    int x = 0;
    int y = ry;
    int rx2 = rx * rx;
    int ry2 = ry * ry;
    int twoRx2 = 2 * rx2;
    int twoRy2 = 2 * ry2;

    // Región 1
    int p = round(ry2 - (rx2 * ry) + (0.25 * rx2));
    int px = 0;
    int py = twoRx2 * y;

    while (px < py) {
        drawPixel(center.x + x, center.y + y);
        drawPixel(center.x - x, center.y + y);
        drawPixel(center.x + x, center.y - y);
        drawPixel(center.x - x, center.y - y);

        x++;
        px += twoRy2;

        if (p < 0) {
            p += ry2 + px;
        } else {
            y--;
            py -= twoRx2;
            p += ry2 + px - py;
        }
    }

    // Región 2
    p = round(ry2 * (x + 0.5) * (x + 0.5) + rx2 * (y - 1) * (y - 1) - rx2 * ry2);

    while (y >= 0) {
        drawPixel(center.x + x, center.y + y);
        drawPixel(center.x - x, center.y + y);
        drawPixel(center.x + x, center.y - y);
        drawPixel(center.x - x, center.y - y);

        y--;
        py -= twoRx2;

        if (p > 0) {
            p += rx2 - py;
        } else {
            x++;
            px += twoRy2;
            p += rx2 - py + px;
        }
    }
}

// Funciones de dibujo auxiliares
void drawGrid() {
    glColor3f(0.9f, 0.9f, 0.9f);  // gris claro
    glBegin(GL_LINES);
    for (int x = -WIDTH/2; x <= WIDTH/2; x += GRID_SPACING) {
        glVertex2i(x, -HEIGHT/2);
        glVertex2i(x, HEIGHT/2);
    }
    for (int y = -HEIGHT/2; y <= HEIGHT/2; y += GRID_SPACING) {
        glVertex2i(-WIDTH/2, y);
        glVertex2i(WIDTH/2, y);
    }
    glEnd();
}

void drawAxes() {
    glColor3f(0.0f, 0.0f, 0.0f);  // negro
    glBegin(GL_LINES);
    // Eje X
    glVertex2i(-WIDTH/2, 0);
    glVertex2i(WIDTH/2, 0);
    // Eje Y
    glVertex2i(0, -HEIGHT/2);
    glVertex2i(0, HEIGHT/2);
    glEnd();
}


void displayCoordinates() {
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    gluOrtho2D(0, WIDTH, HEIGHT, 0);
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();

    glColor3f(1.0f, 1.0f, 1.0f);
    glRasterPos2i(10, 20);
    string coords = "X: " + to_string(mouseX - WIDTH/2) + " Y: " + to_string(HEIGHT/2 - mouseY);
    for (char c : coords) {
        glutBitmapCharacter(GLUT_BITMAP_9_BY_15, c);
    }

    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
}

// Callbacks de OpenGL
void display() {
    glClear(GL_COLOR_BUFFER_BIT);

    if (showGrid) drawGrid();
    if (showAxes) drawAxes();

    // Dibujar todas las figuras
    for (const auto& figure : figures) {
        glColor3fv(figure.color);
        currentThickness = figure.thickness;

        switch (figure.type) {
            case 0: // Recta directo
                if (figure.points.size() >= 2)
                    drawLineDirect(figure.points[0], figure.points[1]);
                break;
            case 1: // Recta DDA
                if (figure.points.size() >= 2)
                    drawLineDDA(figure.points[0], figure.points[1]);
                break;
            case 2: // Círculo incremental
                if (figure.points.size() >= 2) {
                    int dx = figure.points[1].x - figure.points[0].x;
                    int dy = figure.points[1].y - figure.points[0].y;
                    int radius = (int)sqrt(dx*dx + dy*dy);
                    drawCircleIncremental(figure.points[0], radius);
                }
                break;
            case 3: // Círculo PM
                if (figure.points.size() >= 2) {
                    int dx = figure.points[1].x - figure.points[0].x;
                    int dy = figure.points[1].y - figure.points[0].y;
                    int radius = (int)sqrt(dx*dx + dy*dy);
                    drawCircleMidpoint(figure.points[0], radius);
                }
                break;
            case 4: // Elipse PM
                if (figure.points.size() >= 3) {
                    int rx = abs(figure.points[1].x - figure.points[0].x);
                    int ry = abs(figure.points[2].y - figure.points[0].y);
                    drawEllipseMidpoint(figure.points[0], rx, ry);
                }
                break;
        }
    }

    // Dibujar puntos temporales
    glColor3f(1.0f, 0.0f, 0.0f);
    glPointSize(5);
    glBegin(GL_POINTS);
    for (int i = 0; i < pointCount; i++) {
        glVertex2i(tempPoints[i].x, tempPoints[i].y);
    }
    glEnd();

    if (showCoords) displayCoordinates();

    glutSwapBuffers();
}

void mouse(int button, int state, int x, int y) {
    if (button == GLUT_LEFT_BUTTON && state == GLUT_DOWN) {
        mouseX = x; mouseY = y;

        // Convertir coordenadas de ventana a coordenadas OpenGL
        Point p(x - WIDTH/2, HEIGHT/2 - y);

        if (pointCount < 3) {
            tempPoints[pointCount++] = p;
        }

        // Verificar si tenemos suficientes puntos para dibujar
        if ((currentTool <= 1 && pointCount == 2) || // Rectas
            ((currentTool == 2 || currentTool == 3) && pointCount == 2) || // Círculos
            (currentTool == 4 && pointCount == 3)) { // Elipses

            Figure newFig;
            newFig.type = currentTool;
            newFig.thickness = currentThickness;
            memcpy(newFig.color, currentColor, sizeof(currentColor));

            for (int i = 0; i < pointCount; i++) {
                newFig.points.push_back(tempPoints[i]);
            }

            figures.push_back(newFig);
            pointCount = 0;
        }

        glutPostRedisplay();
    }
}

void savePNG(const char* filename, int width, int height) {
    vector<unsigned char> pixels(3 * width * height);
    glReadPixels(0, 0, width, height, GL_RGB, GL_UNSIGNED_BYTE, pixels.data());

    // Invertir en vertical
    vector<unsigned char> flipped(3 * width * height);
    for (int y = 0; y < height; y++) {
        memcpy(&flipped[y * width * 3],
               &pixels[(height - 1 - y) * width * 3],
               width * 3);
    }

    int success = stbi_write_png(filename, width, height, 3, flipped.data(), width * 3);
    if (success) {
        cout << "Imagen exportada como " << filename << endl;
    } else {
        cout << "Error al exportar PNG" << endl;
    }
}

void keyboard(unsigned char key, int x, int y) {
    switch (key) {
        case 'g': case 'G':
            showGrid = !showGrid;
            break;
        case 'e': case 'E':
            showAxes = !showAxes;
            break;
        case 'c': case 'C':
            figures.clear();
            pointCount = 0;
            break;
        case 'z': case 'Z':
            if (!figures.empty()) {
                undoStack.push_back(figures.back());
                figures.pop_back();
            }
            break;
        case 'y': case 'Y':
            if (!undoStack.empty()) {
                figures.push_back(undoStack.back());
                undoStack.pop_back();
            }
            break;
        case 's': case 'S':
        cout << "Exportando imagen..." << endl;
        savePNG("C:/Users/Usuario/Desktop/captura.png", WIDTH, HEIGHT);
   // ✅ Exportar como PNG
        break;


        }
        glutPostRedisplay();
}

// Menús
void mainMenu(int value) {
    // Menú principal ya manejado por submenús
}

void drawingMenu(int value) {
    currentTool = value;
    pointCount = 0;
    cout << "Tool selected: " << value << endl;
}

void colorMenu(int value) {
    switch (value) {
        case 0: currentColor[0] = 0.0f; currentColor[1] = 0.0f; currentColor[2] = 0.0f; break; // Negro
        case 1: currentColor[0] = 1.0f; currentColor[1] = 0.0f; currentColor[2] = 0.0f; break; // Rojo
        case 2: currentColor[0] = 0.0f; currentColor[1] = 1.0f; currentColor[2] = 0.0f; break; // Verde
        case 3: currentColor[0] = 0.0f; currentColor[1] = 0.0f; currentColor[2] = 1.0f; break; // Azul
        case 4: { // Personalizado
            int r, g, b;
            cout << "Ingrese color personalizado (RGB 0-255): ";
            cin >> r >> g >> b;
            currentColor[0] = r / 255.0f;
            currentColor[1] = g / 255.0f;
            currentColor[2] = b / 255.0f;
            break;
        }
    }
}

void thicknessMenu(int value) {
    currentThickness = value;
}

void viewMenu(int value) {
    switch (value) {
        case 0: showGrid = !showGrid; break;
        case 1: showAxes = !showAxes; break;
        case 2: showCoords = !showCoords; break;
    }
}

void toolsMenu(int value) {
    switch (value) {
        case 0: figures.clear(); pointCount = 0; break;  // Limpiar lienzo
        case 1: // Borrar última figura
            if (!figures.empty()) {
                undoStack.push_back(figures.back());
                figures.pop_back();
            }
            break;
        case 2: // Exportar imagen
            savePNG("captura.png", WIDTH, HEIGHT);
            break;
    }
    glutPostRedisplay();
}


void helpMenu(int value) {
    if (value == 0) {
        cout << "Atajos de teclado:" << endl;
        cout << "G - Mostrar/Ocultar cuadrícula" << endl;
        cout << "E - Mostrar/Ocultar ejes" << endl;
        cout << "C - Limpiar lienzo" << endl;
        cout << "Z - Deshacer" << endl;
        cout << "Y - Rehacer" << endl;
        cout << "S - Exportar imagen" << endl;
    }
}

void createMenu() {
    int drawSubMenu = glutCreateMenu(drawingMenu);
    glutAddMenuEntry("Recta (Directo)", 0);
    glutAddMenuEntry("Recta (DDA)", 1);
    glutAddMenuEntry("Circulo (Incremental)", 2);
    glutAddMenuEntry("Circulo (Punto Medio)", 3);
    glutAddMenuEntry("Elipse (Punto Medio)", 4);

    int colorSubMenu = glutCreateMenu(colorMenu);
    glutAddMenuEntry("Negro", 0);
    glutAddMenuEntry("Rojo", 1);
    glutAddMenuEntry("Verde", 2);
    glutAddMenuEntry("Azul", 3);
    glutAddMenuEntry("Personalizado...", 4);

    int thicknessSubMenu = glutCreateMenu(thicknessMenu);
    glutAddMenuEntry("1 px", 1);
    glutAddMenuEntry("2 px", 2);
    glutAddMenuEntry("3 px", 3);
    glutAddMenuEntry("5 px", 5);

    int viewSubMenu = glutCreateMenu(viewMenu);
    glutAddMenuEntry("Mostrar/Ocultar cuadricula", 0);
    glutAddMenuEntry("Mostrar/Ocultar ejes", 1);
    glutAddMenuEntry("Mostrar coordenadas", 2);

    int toolsSubMenu = glutCreateMenu(toolsMenu);
    glutAddMenuEntry("Limpiar lienzo", 0);
    glutAddMenuEntry("Deshacer", 1);
    glutAddMenuEntry("Exportar imagen (PNG)", 2);

    int helpSubMenu = glutCreateMenu(helpMenu);
    glutAddMenuEntry("Atajos de teclado", 0);
    glutAddMenuEntry("Acerca de", 1);

    glutCreateMenu(mainMenu);
    glutAddSubMenu("Dibujo", drawSubMenu);
    glutAddSubMenu("Color", colorSubMenu);
    glutAddSubMenu("Grosor", thicknessSubMenu);
    glutAddSubMenu("Vista", viewSubMenu);
    glutAddSubMenu("Herramientas", toolsSubMenu);
    glutAddSubMenu("Ayuda", helpSubMenu);

    glutAttachMenu(GLUT_RIGHT_BUTTON);
}

void init() {
    glClearColor(1.0f, 1.0f, 1.0f, 1.0f);  // FONDO BLANCO
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluOrtho2D(-WIDTH/2, WIDTH/2, -HEIGHT/2, HEIGHT/2);
    glMatrixMode(GL_MODELVIEW);
}


int main(int argc, char** argv) {
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB);
    glutInitWindowSize(WIDTH, HEIGHT);
    glutInitWindowPosition(100, 100);
    glutCreateWindow("CAD 2D Basic - OpenGL/FreeGLUT");

    init();
    createMenu();

    glutDisplayFunc(display);
    glutMouseFunc(mouse);
    glutKeyboardFunc(keyboard);
    glutPassiveMotionFunc([](int x, int y) {
        mouseX = x; mouseY = y;
        if (showCoords) glutPostRedisplay();
    });

    cout << "CAD 2D Basic inicializado" << endl;
    cout << "Click derecho para menu contextual" << endl;
    cout << "Atajos: G (grid), E (ejes), C (clear), Z (undo), Y (redo)" << endl;

    glutMainLoop();
    return 0;
}
