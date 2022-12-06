#!/usr/bin/env python
# Gradient boosting in 100 lines or less.
import random, math, sys

class Node(object):
    def __init__(self, instances, nminobs):
        self.instances = instances                          # (x,y,w,a) list: x=features, y=target, w=weight, a=aux data
        W = float(sum(w for (x, y, w, a) in instances))
        self.var, self.left, self.right = None, None, None  # split variable (None if leaf), and child nodes
        self.value = sum(w*y for (x, y, w, a) in instances) / (1 if (W == 0) else W)  # split point, or output of a leaf
        self.best_split = (0.0, None, 0.0)                  # best split's (gain, split variable, split value)

        if W < 2 * nminobs: return

        # find the split which produces largest reduction in CART training error (weighted sum of squared errors)
        for var in range(len(instances[0][0])):
            values = sorted([(x[var], y, w) for (x, y, w, a) in instances if w > 0])
            total_sum = float(sum(w*y for (x, y, w) in values))
            lwei, rwei = 0.0, W
            lsum, rsum = 0.0, total_sum

            # consider all split points in the middle between two adjacent values of attribute var in the training set
            for i, (x, y, w) in enumerate(values):
                lwei, rwei = lwei + w, rwei - w        # shift one observation from right node to left
                lsum, rsum = lsum + w*y, rsum - w*y
                if lwei < nminobs or rwei < nminobs: continue

                next_x = values[i+1][0]
                if next_x == x: continue  # instances with equal attribute should all go to the same child node

                gain = -total_sum**2/W + lsum**2/lwei + rsum**2/rwei  # decrease in training error
                if gain > self.best_split[0]:
                    self.best_split = (gain, var, (x + next_x) / 2)

    def do_split(self, nminobs):  # called when we decide to proceed with the best split (which was found in __init__)
        self.var, self.value = self.best_split[1:]
        self.left = Node([x for x in self.instances if x[0][self.var] < self.value], nminobs)
        self.right = Node([x for x in self.instances if not (x[0][self.var] < self.value)], nminobs)
        self.instances = None
        return (self.left, self.right)


class RegressionTree(object):
    """CART regression tree, without pruning and missing-attributes stuff. Handles just real-valued attributes."""

    def __init__(self, instances, nleaves, nminobs):
        self.root = Node(instances, nminobs)
        self.leaves = [self.root]
        while len(self.leaves) < nleaves:
            node = max(self.leaves, key=lambda n: n.best_split[0])
            if node.best_split[0] <= 0: return  # no more splits are possible
            self.leaves.remove(node)
            self.leaves += node.do_split(nminobs)

    def __call__(self, x):  # evaluation
        node = self.root
        while node.var is not None:
            if x[node.var] < node.value:
                node = node.left
            else:
                node = node.right
        return node.value


class TreeBoost(object):
    """Stochastic gradient tree boosting method."""

    # Default parameters and loss function specification
    L2_LOSS = dict(deriv=lambda y,f: f-y, gamma=lambda y,f: sum(y[i]-f[i] for i in range(len(y))) / len(y))
    L1_LOSS = dict(deriv=lambda y,f: -(f<y)+(f>y), gamma=lambda y,f: sorted([y[i]-f[i] for i in range(len(y))])[len(y)//2])
    DEFAULTS = dict(ntrees=20, nleaves=5, shrinkage=0.1, nminobs=10, loss=L2_LOSS)

    def __init__(self, **params):
        self.__dict__.update(self.DEFAULTS)
        self.__dict__.update(params)
        self.loss_deriv, self.argmin_gamma = self.loss['deriv'], self.loss['gamma']

    def fit(self, data):
        self.bias = self.argmin_gamma([y for (x, y) in data], [0] * len(data))  # best constant approximation
        self.F = [self.bias] * len(data)                      # vector of current predictions at each instance
        self.trees = [self.fit_tree(data) for _ in range(self.ntrees)]

    def fit_tree(self, data):
        # build the tree on a random subsample - here we subsample by assigning 0 weights
        subsample = [(x, -self.loss_deriv(y, self.F[i]), random.randint(0, 1), i) for i, (x, y) in enumerate(data)]
        tree = RegressionTree(subsample, nleaves=self.nleaves, nminobs=self.nminobs)

        # optimize loss in each tree's region (hence this is gradient *tree* boost)
        for leaf in tree.leaves:
            idx = [a for (x, y, w, a) in leaf.instances]  # indices of instances (including 0-weighted) in this leaf
            leaf.instances = None                         # free memory
            leaf.value = self.shrinkage * self.argmin_gamma([data[i][1] for i in idx], [self.F[i] for i in idx])
            for i in idx:
                self.F[i] += leaf.value                   # update predictions at instances

        return tree

    def predict(self, x):  # evaluation
        return self.bias + sum(tree(x) for tree in self.trees)
