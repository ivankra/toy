# Gradient boosting in 100 lines or less

Sample usage:

```py
from gb100 import TreeBoost

model = TreeBoost(ntrees=20)
model.fit([(features, target), ...])
model.predict(features)
```

Parameters:
  * ntrees: a positive integer (= gbm's n.trees)
      Number of trees in the model.
  * shrinkage: a positive real number (= gbm's shrinkage)
      Learning rate.
      Hastie et al. recommend setting it to a small value < 0.1.
      Decrease of shrinkage should be compensated by an increase of ntrees.
  * nleaves: a positive integer, at least 2. (= gbm's interaction.depth + 1)
      Number of leaves (terminal nodes) in each tree,
      For example, nleaves=2 produces decision stumps.
      Hastie et al. recommend nleaves=4..8, and note that "it is unlikely that
      [values of ntrees] > 10 will be required".
  * nminobs: a positive integer (= gbm's n.minobsinnode)
      Minimum number of training instances that should be contained in each
      tree leaf (i.e. CART code will ignore splits leading to nodes with
      fewer than nminobs observations)
  * loss: specification of the loss function L(y, y'), where y' = f(x) is
    the predicted value. This is a Python dict with two function objects:
      * deriv(y, f) = derivative of L(y, f) with respect to f  (y, f   are reals)
      * gamma(y, F) = $\arg\min_{\gamma} \sum_i L(y[i], F[i] + \gamma)$  (y, F are vectors)

    See Friedman's original paper for some worked-out examples.
      * TreeBoost.L2_LOSS is least squares loss L(y, y') = 1/2 (y - y')^2
      * TreeBoost.L1_LOSS is least absolute deviation loss L(y, y') = |y - y'|

Notes about this particular implementation of gradient boosting:
  * subsampling is done by flipping a fair coin once for each instance.
    Note that this is different from Friedman's pseudocode.
    It's more compact python-wise, and seems to produce a bit better models.
  * CART code sorts training set once for each feature on each split.
    This adds a rather wasteful O(log n) factor to the runtime as compared
    to industrial implementations.
  * CART code is written for real-values attributes only. It'll naturally
    handle binary attributes though, but you'll likely need to manually
    preprocess more general categorical attributes.

References:
  * Friedman J. (1999). Greedy Function Approximation: A Gradient Boosting machine.  (the original paper)
  * Friedman J. (1999). Stochastic Gradient Boosting.  (describes the important subsampling trick for g.b.)
  * Hastie T., Tibshirani R., Friedman J. (2009). The Elements of Statistical Learning.  (a good, modern textbook)
  * Ridgeway, G. ( ...)  (overview of boosting models)
  * http://cran.r-project.org/web/packages/gbm/index.html  (an open-source industrial implementation of g.b.)
