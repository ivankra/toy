package viz;
import java.util.*;
import java.awt.geom.*;

public class PointSet implements Iterable<Point2D.Double> {
    public interface Listener {
        public void pointSetChanged();
        public void pointMoved(Point2D.Double p, Point2D.Double oldpos);
    }

    private ArrayList<Point2D.Double> points = new ArrayList<Point2D.Double>();
    private ArrayList<Listener> listeners = new ArrayList<Listener>();

    public PointSet() {}
    public PointSet(double... a) {
        for (int i = 0; i + 1 < a.length; i += 2)
            add(a[i], a[i+1]);
    }

    public void add(double x, double y) {
        points.add(new Point2D.Double(x, y));
        fireSetChanged();
    }

    public void remove(Point2D.Double p) {
        points.remove(p);
        fireSetChanged();
    }

    public boolean contains(Point2D.Double p) {
        return points.contains(p);
    }

    public void change(Point2D.Double p, double newX, double newY) {
        Point2D.Double old = new Point2D.Double(p.x, p.y);
        p.setLocation(newX, newY);
        for (Listener l : listeners)
            l.pointMoved(p, old);
        fireSetChanged();
    }

    public void clear() {
        if (points.size() != 0) {
            points.clear();
            fireSetChanged();
        }
    }

    public Point2D.Double[] getPoints() {
        return points.toArray(new Point2D.Double[0]);
    }
    
    public int size() {
        return points.size();
    }
    
    public Point2D.Double get(int i) { return points.get(i); }
    public double getX(int i) { return get(i).x; }
    public double getY(int i) { return get(i).y; }

    public Iterator<Point2D.Double> iterator() {
        return points.iterator();
    }

    public Point2D.Double getSelectedPoint(int ix, int iy, View2D.ViewTransform transform) {
        for (Point2D.Double p : points) {
            Point2D.Double q = transform.project(p);
            if (q == null) continue;
            int d = 10;
            if (Math.abs(q.x - ix) < d && Math.abs(q.y - iy) < d)
                return p;
        }
        return null;
    }

    public void addListener(Listener l) {
        listeners.add(l);
    }

    private void fireSetChanged() {
        for (Listener l : listeners)
            l.pointSetChanged();
    }
}
