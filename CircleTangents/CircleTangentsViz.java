import java.awt.*;
import java.awt.geom.*;
import javax.swing.*;
import java.util.*;
import viz.*;

public class CircleTangentsViz
    implements Runnable, View2D.Renderable, PointSet.Listener {

    PointSet pointSet = new PointSet(-5,0, 1.5,0, 7,0, 9,0);

    public void draw(Graphics2D g, View2D.ViewTransform transform) {
        double x1 = pointSet.getX(0), y1 = pointSet.getY(0);
        double r1 = pointSet.get(0).distance(pointSet.get(1));

        double x2 = pointSet.getX(2), y2 = pointSet.getY(2);
        double r2 = pointSet.get(2).distance(pointSet.get(3));

        g.setColor(Color.BLUE);
        g.draw(new Path2D.Double(new Ellipse2D.Double(x1-r1, y1-r1, r1*2, r1*2), transform));
        g.draw(new Path2D.Double(new Ellipse2D.Double(x2-r2, y2-r2, r2*2, r2*2), transform));

        double[][] res = CircleTangents.getTangents(x1, y1, r1, x2, y2, r2);
        if (res == null) return;

        for (int i = 0; i < res.length; i++) {
            double a[] = res[i];
            Path2D.Double p = new Path2D.Double();
            p.moveTo(a[0], a[1]);
            p.lineTo(a[2], a[3]);
            p.transform(transform);

            g.setColor(i <= 1 ? Color.RED : Color.MAGENTA);
            g.draw(p);
            
            for (int j = 0; j <= 2; j += 2) {
                Point2D.Double q = new Point2D.Double();
                transform.transform(new Point2D.Double(a[j], a[j+1]), q);
                g.fill(new Ellipse2D.Double(q.x - 3, q.y - 3, 6, 6));
            }
        }
    }

    // A custom listener so that moving the center just moves the whole circle
    public void pointSetChanged() {}
    public void pointMoved(Point2D.Double p, Point2D.Double oldpos) {
        double dx = p.x - oldpos.x, dy = p.y - oldpos.y;
        if (p == pointSet.get(0))
            pointSet.change(pointSet.get(1), pointSet.getX(1) + dx, pointSet.getY(1) + dy);
        if (p == pointSet.get(2))
            pointSet.change(pointSet.get(3), pointSet.getX(3) + dx, pointSet.getY(3) + dy);
    }

    public void run() {
        pointSet.addListener(this);

        View2D v = new View2D(pointSet);
        v.setPermittedPointsActions(View2D.POINTS_MOVE);
        v.setRenderable(this);
        v.setNodeColor(Color.BLUE);

        JPanel p = new JPanel(new BorderLayout());
        p.add(v, BorderLayout.CENTER);

        JFrame w = new JFrame("Circle Tangents Visualizer");
        w.setContentPane(p);
        w.pack();
        w.setDefaultCloseOperation(JFrame.EXIT_ON_CLOSE);
        w.setVisible(true);                
    }

    public static void main(String[] args) {
        SwingUtilities.invokeLater(new CircleTangentsViz());
    }
}
