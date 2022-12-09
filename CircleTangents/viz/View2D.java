package viz;
import java.util.*;
import java.awt.*;
import java.awt.event.*;
import java.awt.geom.*;
import javax.swing.*;

public class View2D extends JComponent
        implements MouseListener, MouseMotionListener, MouseWheelListener,
        PointSet.Listener
{
    public interface Renderable {
        public void draw(Graphics2D g, ViewTransform transform);
    }

    private static final Color COLOR_BACKGROUND = Color.WHITE;
    private static final Color COLOR_RULER = Color.BLACK;
    private static final Color COLOR_GRID_1 = Color.LIGHT_GRAY;
    private static final Color COLOR_GRID_2 = new Color(224, 224, 224);
    private static final Color selectedNodeColor = new Color(224, 190, 0);
    private Color nodeColor = Color.RED;

    public static final int POINTS_MOVE = 1;
    public static final int POINTS_MODIFY = 2;    // add/remove with a right click
    private int permittedPointsActions = POINTS_MOVE | POINTS_MODIFY;

    private ViewTransform transform;
    private Renderable[] renderables;
    private PointSet pointSet;
    private Point2D.Double selectedPoint;
    private boolean showGrid = true;

    private static final int MOUSE_NORMAL = 1;
    private static final int MOUSE_TRANSLATE = 2;
    private static final int MOUSE_MOVE = 3;
    private int mouseState = MOUSE_NORMAL;
    private Point lastMousePos = null;

    /**
     * Represents a transformation from model space to screen space:
     *   x -> (x - minX) / scaleX
     *   y -> height - (y - minY) / scaleY
     */
    public static class ViewTransform extends AffineTransform {
        public double minX, minY, scaleX, scaleY, width, height;

        public ViewTransform(double minX, double minY,
                             double scaleX, double scaleY,
                             double width, double height) {
            super(1/scaleX, 0, 0, -1/scaleY, -minX/scaleX, height+minY/scaleY);
            this.minX = minX;
            this.minY = minY;
            this.scaleX = scaleX;
            this.scaleY = scaleY;
            this.width = Math.max(1, width);
            this.height = Math.max(1, height);
        }

        public double getMinX() { return minX; }
        public double getMinY() { return minY; }
        public double getMaxX() { return minX + width * scaleX; }
        public double getMaxY() { return minY + height * scaleY; }

        public boolean isVisible(double x, double y) {
            return x >= minX && y >= minY && x <= getMaxX() && y <= getMaxY();
        }

        public Point2D.Double project(Point2D.Double p) {
            //if (!isVisible(p.x, p.y)) return null;
            return new Point2D.Double(
                    (p.x - minX) / scaleX,
                    height - (p.y - minY) / scaleY);
        }

        public int projectX(double x) {
            return (int)Math.round((x - minX) / scaleX);
        }

        public int projectY(double y) {
            return (int)Math.round(height - (y - minY) / scaleY);
        }

        public Point2D.Double unproject(int x, int y) {
            double xx = minX + x * scaleX;
            double yy = minY + (height - y) * scaleY;
            return new Point2D.Double(xx, yy);
        }

        public ViewTransform resize(double newWidth, double newHeight) {
            return new ViewTransform(minX, minY, scaleX, scaleY, newWidth, newHeight);
        }

        public ViewTransform shift(int dx, int dy) {
            return new ViewTransform(
                    minX - dx * scaleX, minY + dy * scaleY,
                    scaleX, scaleY, width, height);
        }

        public ViewTransform zoom(int x0, int y0, int wheelRotation) {
            return this.zoomX(wheelRotation).zoomY(wheelRotation);
        }

        public ViewTransform zoomX(int dir) {
            double z = Math.pow(1.10, dir);
            double mx = minX + (1-z)/2 * scaleX * width;
            return new ViewTransform(mx, minY, z * scaleX, scaleY, width, height);
        }

        public ViewTransform zoomY(int dir) {
            double z = Math.pow(1.10, dir);
            double my = minY + (1-z)/2 * scaleY * height;
            return new ViewTransform(minX, my, scaleX, z * scaleY, width, height);
        }
    }

    public View2D(PointSet pointSet) {
        this.pointSet = pointSet;
        this.pointSet.addListener(this);

        setPreferredSize(new Dimension(640, 480));
        setSize(getPreferredSize());
        setViewRegion(-10 * 4.0/3.0, 10 * 4.0/3.0, -10, 10);

        addMouseListener(this);
        addMouseWheelListener(this);
        addMouseMotionListener(this);
        addComponentListener(new ComponentAdapter() {
            @Override
            public void componentResized(ComponentEvent e) {
                transform = transform.resize(getWidth(), getHeight());
            }
        });
    }

    public boolean getShowGrid() {
        return showGrid;
    }

    public void setShowGrid(boolean s) {
        if (showGrid != s) {
            showGrid = s;
            repaint();
        }
    }
    
    public void setPermittedPointsActions(int a) {
        permittedPointsActions = a;
    }

    public Color getNodeColor() {
        return nodeColor;
    }

    public void setNodeColor(Color color) {
        nodeColor = color;
        repaint();
    }

    public void setRenderables(Renderable[] p) {
        renderables = p;
        repaint();
    }

    public void setRenderable(Renderable p) {
        setRenderables(new Renderable[] { p });
    }

    public ViewTransform getTransform() {
        return transform;
    }
    
    public void setTransform(ViewTransform t) {
        transform = t;
        repaint();
    }

    public void setViewRegion(double minX, double maxX, double minY, double maxY) {
        double scaleX = (maxX - minX) / getWidth();
        double scaleY = (maxY - minX) / getHeight();
        transform = new ViewTransform(minX, minY, scaleX, scaleY, getWidth(), getHeight());
    }

    @Override
    public void paintComponent(Graphics gg) {
        Graphics2D g = (Graphics2D)gg;
        transform = transform.resize(getWidth(), getHeight());

        g.setColor(COLOR_BACKGROUND);
        g.fillRect(0, 0, getWidth(), getHeight());

        g.setRenderingHint(RenderingHints.KEY_ANTIALIASING, 
            RenderingHints.VALUE_ANTIALIAS_ON);

        drawGridAndRuler(g);

        g.setColor(COLOR_RULER);
        g.drawLine(0, 0, 0, getHeight());
        g.drawLine(0, getHeight()-1, getWidth(), getHeight()-1);

        if (renderables != null) {
            for (Renderable p : renderables) {
                g.setColor(Color.BLACK);
                g.setStroke(new BasicStroke(2.0f, BasicStroke.CAP_ROUND, BasicStroke.JOIN_MITER));
                p.draw(g, transform);
            }
        }

        if (pointSet != null) {
            for (Point2D.Double p : pointSet)
                drawPoint(g, p, false);
        }
    }

    private void drawGridAndRuler(Graphics2D g) {
        double[] a = new double[] { transform.getMinX(), transform.getMinY() };
        double[] b = new double[] { transform.getMaxX(), transform.getMaxY() };

        double[] division = new double[2];
        for (int i = 0; i < 2; i++) {
            division[i] = 1;
            while (Math.floor((b[i] - a[i]) / division[i]) == 0) division[i] /= 5.0;
            while (Math.floor((b[i] - a[i]) / division[i]) > 1) division[i] *= 5.0;
            division[i] /= 5.0;
        }

        double ratio = (b[1]-a[1])/(b[0]-a[0]);
        if (0.2 < ratio && ratio < 1/0.2) {
            division[0] = division[1] = Math.min(division[0], division[1]);
        }

        for (int iter = 0; iter < 4; iter++) {
            for (int k = 0; k < 2; k++) {
                outer: for (double z = Math.floor(a[k] / division[k]); ; z += 1) {
                    for (int j = 0; j < (iter <= 1 ? 5 : 1); j++) {
                        double x = (z * 5 + j) * division[k] / 5;
                        if (x > b[k]) break outer;
                        
                        if (k == 0) {
                            int pos = transform.projectX(x);
                            if ((iter == 0 && j != 0) || (iter == 1 && j == 0)) {
                                if (showGrid) {
                                    g.setColor(j == 0 ? COLOR_GRID_1 : COLOR_GRID_2);
                                    g.drawLine(pos, 0, pos, getHeight());
                                }
                            } else if (iter == 2) {
                                g.setColor(COLOR_RULER);
                                g.drawLine(pos, getHeight(), pos, getHeight() - (j == 0 ? 10 : 5));
                            } else if (iter == 3) {
                                if (j == 0) {
                                    g.setColor(COLOR_RULER);
                                    g.drawString(formatRulerLabel(x), pos + 3, getHeight() - 10);
                                }
                            }
                        } else {
                            int pos = transform.projectY(x);
                            if ((iter == 0 && j != 0) || (iter == 1 && j == 0)) {
                                if (showGrid) {
                                    g.setColor(j == 0 ? COLOR_GRID_1 : COLOR_GRID_2);
                                    g.drawLine(0, pos, getWidth(), pos);
                                }
                            } else if (iter == 2) {
                                g.setColor(COLOR_RULER);
                                g.drawLine(0, pos, j == 0 ? 10 : 5, pos);
                            } else if (iter == 3) {
                                if (j == 0) {
                                    g.setColor(COLOR_RULER);
                                    g.drawString(formatRulerLabel(x), 10, pos - 3);
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    private void drawPoint(Graphics2D g, Point2D.Double p, boolean printing) {
        if (transform.isVisible(p.x, p.y)) {
            Point2D.Double q = transform.project(p);
            if (q == null) return;
            if (selectedPoint == p && !printing)
                g.setColor(selectedNodeColor);
            else
                g.setColor(nodeColor);
            g.fill(new Ellipse2D.Double(q.x - 3, q.y - 3, 6, 6));
        }
    }

    private String formatRulerLabel(double x) {
        String s = String.format("%.6f", x);
        char[] c = s.toCharArray();
        int n = c.length;
        while (c[n-1] == '0') n--;
        if (c[n-1] == '.') n--;
        return new String(c, 0, n);
    }

    public void mouseClicked(MouseEvent e) {}
    public void mouseEntered(MouseEvent e) {}
    public void mouseExited(MouseEvent e) {}

    public void mousePressed(MouseEvent e) {
        lastMousePos = new Point(e.getX(), e.getY());

        if (e.getButton() == MouseEvent.BUTTON1) {
            selectedPoint = pointSet.getSelectedPoint(e.getX(), e.getY(), transform);
            if (selectedPoint == null) {
                mouseState = MOUSE_TRANSLATE;
                setCursor(new Cursor(Cursor.DEFAULT_CURSOR));
            } else if ((permittedPointsActions & POINTS_MOVE) != 0) {
                mouseState = MOUSE_MOVE;
                setCursor(new Cursor(Cursor.HAND_CURSOR));
            }
            repaint();
        } else if ((e.getButton() == MouseEvent.BUTTON3 && mouseState == MOUSE_NORMAL) &&
                   ((permittedPointsActions & POINTS_MODIFY) != 0)) {
            selectedPoint = pointSet.getSelectedPoint(e.getX(), e.getY(), transform);
            if (selectedPoint == null) {
                Point2D.Double p = transform.unproject(e.getX(), e.getY());
                pointSet.add(p.x, p.y);
            } else {
                pointSet.remove(selectedPoint);
                selectedPoint = null;
            }
            selectedPoint = pointSet.getSelectedPoint(e.getX(), e.getY(), transform);
            repaint();
        }
    }

    public void mouseReleased(MouseEvent e) {
        selectedPoint = pointSet.getSelectedPoint(e.getX(), e.getY(), transform);
        resetMouseState();
    }

    public void mouseDragged(MouseEvent e) {
        if (mouseState == MOUSE_NORMAL || lastMousePos == null) {
            lastMousePos = new Point(e.getX(), e.getY());
            return;
        }

        int dx = e.getX() - lastMousePos.x;
        int dy = e.getY() - lastMousePos.y;
        if (dx == 0 && dy == 0) return;

        lastMousePos = new Point(e.getX(), e.getY());

        if (mouseState == MOUSE_TRANSLATE) {
            transform = transform.shift(dx, dy);
            repaint();
        } else if (mouseState == MOUSE_MOVE && selectedPoint != null) {
            double newX = selectedPoint.getX() + dx * transform.scaleX; //FIXME
            double newY = selectedPoint.getY() - dy * transform.scaleY;
            pointSet.change(selectedPoint, newX, newY);
            repaint();
        }
    }

    public void mouseMoved(MouseEvent e) {
        lastMousePos = new Point(e.getX(), e.getY());
        if (mouseState == MOUSE_NORMAL) {
            Point2D.Double p = pointSet.getSelectedPoint(e.getX(), e.getY(), transform);
            if (p != selectedPoint) {
                selectedPoint = p;
                repaint();
            }
        }
    }

    public void mouseWheelMoved(MouseWheelEvent e) {
        if (e.getScrollAmount() != 0) {
            transform = transform.zoom(e.getX(), e.getY(), e.getWheelRotation());
            selectedPoint = null;
            resetMouseState();
            repaint();
        }
    }

    public void pointSetChanged() {
        if (selectedPoint != null && !pointSet.contains(selectedPoint)) {
            selectedPoint = null;
            resetMouseState();
        }
        repaint();
    }

    public void pointMoved(Point2D.Double p, Point2D.Double oldpos) {}

    private void resetMouseState() {
        mouseState = MOUSE_NORMAL;
        setCursor(new Cursor(Cursor.DEFAULT_CURSOR));
    }
}
