/**
 * pst.c - Priority Search Tree (PST) Implementation
 *
 * Efficient 2D range queries.
 *
 * Structures:
 *   Pair  - (x, y) coordinate pair
 *   Node  - PST node containing a Pair plus left right children and subtree metadata
 *
 * Functions:
 *   create_pst           - O(n log n) construction from an array of pairs
 *   delete               - O(log n) deletion of a specific pair
 *   enumerate_rectangle  - O(log n + k) query: all (x,y) with x in [xmin,xmax], y <= ymax
 *   special_query        - O(log n + k) angular range query (x treated as angle 0-360)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>

/* ─────────────────────────────────────────────
 * Required Structures
 * ───────────────────────────────────────────── 
 */

typedef struct Pair {
    int x;   /* angle (0–360) in special_query context */
    int y;   /* priority / heap key                    */
} Pair;

typedef struct Node {
    Pair  p;            /* point stored at this node        */
    int   x_min;        /* minimum x in left subtree        */
    int   x_max;        /* maximum x in right subtree       */
    struct Node *left;
    struct Node *right;
} Node;

/* ─────────────────────────────────────────────
 * Helper functions
 * ───────────────────────────────────────────── 
 */


/* Comparison for sorting by x then y for compatability with duplicates */
static int cmp_pair_x(const void *a, const void *b) {
    const Pair *pa = (const Pair *)a;
    const Pair *pb = (const Pair *)b;
    if (pa->x != pb->x) return pa->x - pb->x;
    return pa->y - pb->y;
}

/* Allocate and initialize  Node */
static Node *make_node(Pair p) {
    Node *n = (Node *)malloc(sizeof(Node));
    if (!n) { fprintf(stderr, "malloc failed\n"); exit(1); }
    n->p      = p;
    n->x_min  = p.x;
    n->x_max  = p.x;
    n->left   = NULL;
    n->right  = NULL;
    return n;
}

/**
 * build_pst_recursive : Recursive O(n log n) PST construction.
 *
 * Algorithm:
 *   Find the pair with the max y value and set root (heap property)
 *   Sort remaining pairs by x and split at the median to form left right subtrees
 *   Recurse
 *
 * @param pairs   sorted-by-x array of pairs for this subtree
 * @param n       number of pairs
 * @return        root Node of the constructed subtree
 **/
static Node *build_pst_recursive(Pair *pairs, int n) {
    if (n <= 0) return NULL;

    /* Find index of max y element (heap property) */
    int max_idx = 0;
    for (int i = 1; i < n; i++) {
        if (pairs[i].y > pairs[max_idx].y ||
            (pairs[i].y == pairs[max_idx].y && pairs[i].x < pairs[max_idx].x))
            max_idx = i;
    }

    /* Extract the max y pair as the root */
    Pair root_pair = pairs[max_idx];
    Node *root = make_node(root_pair);

    if (n == 1) return root;

    /* Build remaining pair array (exclude max element) */
    Pair *rest = (Pair *)malloc((n - 1) * sizeof(Pair));
    if (!rest) { fprintf(stderr, "malloc failed\n"); exit(1); }
    int j = 0;
    for (int i = 0; i < n; i++)
        if (i != max_idx) rest[j++] = pairs[i];

    /* rest is sorted by x */
    /* Split at median */
    int left_count  = (n - 1) / 2;
    int right_count = (n - 1) - left_count;

    root->left  = build_pst_recursive(rest,              left_count);
    root->right = build_pst_recursive(rest + left_count, right_count);

    /* Update x range */
    root->x_min = root->p.x;
    root->x_max = root->p.x;
    if (root->left) {
        if (root->left->x_min < root->x_min) root->x_min = root->left->x_min;
        if (root->left->x_max > root->x_max) root->x_max = root->left->x_max;
    }
    if (root->right) {
        if (root->right->x_min < root->x_min) root->x_min = root->right->x_min;
        if (root->right->x_max > root->x_max) root->x_max = root->right->x_max;
    }

    free(rest);
    return root;
}

/* ─────────────────────────────────────────────
 * Required Functions
 * ───────────────────────────────────────────── 
 */

/**
 * @param pairs   input array of n Pairs
 * @param n       number of pairs
 * @return        pointer to the root of the PST
 **/
Node *create_pst(Pair *pairs, int n) {
    if (n <= 0 || !pairs) return NULL;

    Pair *sorted = (Pair *)malloc(n * sizeof(Pair));
    if (!sorted) { fprintf(stderr, "malloc failed\n"); exit(1); }
    memcpy(sorted, pairs, n * sizeof(Pair));
    qsort(sorted, n, sizeof(Pair), cmp_pair_x);

    Node *root = build_pst_recursive(sorted, n);
    free(sorted);
    return root;
}

static void collect_all(Node *root, Pair **arr, int *cnt, int *cap) {
    if (!root) return;
    if (*cnt >= *cap) {
        *cap *= 2;
        *arr  = (Pair *)realloc(*arr, *cap * sizeof(Pair));
    }
    (*arr)[(*cnt)++] = root->p;
    collect_all(root->left,  arr, cnt, cap);
    collect_all(root->right, arr, cnt, cap);
}

static void free_tree(Node *root) {
    if (!root) return;
    free_tree(root->left);
    free_tree(root->right);
    free(root);
}

/**
 * Removes a pair from the PST.
 * @param root   double pointer to tree root (root may change)
 * @param pair   the pair to remove (first occurrence)
 **/
void delete(Node **root, Pair pair) {
    if (!root || !*root) return;

    Node  *parent    = NULL;
    Node  *curr      = *root;
    int    is_left   = 0;

    while (curr) {
        if (curr->p.x == pair.x && curr->p.y == pair.y) break;
        parent  = curr;
        /* Navigate toward the x value */
        if (pair.x <= curr->p.x) {
            is_left = 1;
            curr    = curr->left;
        } else {
            is_left = 0;
            curr    = curr->right;
        }
    }
    if (!curr) return; 

    int   cap = 16, cnt = 0;
    Pair *arr = (Pair *)malloc(cap * sizeof(Pair));
    collect_all(curr->left,  &arr, &cnt, &cap);
    collect_all(curr->right, &arr, &cnt, &cap);

    free_tree(curr->left);
    free_tree(curr->right);
    free(curr);

    Node *new_sub = NULL;
    if (cnt > 0) {
        qsort(arr, cnt, sizeof(Pair), cmp_pair_x);
        new_sub = build_pst_recursive(arr, cnt);
    }
    free(arr);

    if (!parent) {
        *root = new_sub;
    } else if (is_left) {
        parent->left  = new_sub;
    } else {
        parent->right = new_sub;
    }
}

/* ─────────────────────────────────────────────
 * enumerate_rectangle
 * ───────────────────────────────────────────── 
 */

/* Dynamic result array helpers */
typedef struct {
    Pair *data;
    int   cnt;
    int   cap;
} ResultArr;

static void result_push(ResultArr *r, Pair p) {
    if (r->cnt >= r->cap) {
        r->cap  = r->cap ? r->cap * 2 : 8;
        r->data = (Pair *)realloc(r->data, r->cap * sizeof(Pair));
    }
    r->data[r->cnt++] = p;
}

static void enum_rect_recurse(Node *node, int x_min, int x_max, int y_max,
                               ResultArr *res) {
    if (!node) return;

    if (node->x_max < x_min || node->x_min > x_max) return;
    
    if (node->p.y <= y_max &&
        node->p.x >= x_min && node->p.x <= x_max) {
        result_push(res, node->p);
    }
    enum_rect_recurse(node->left,  x_min, x_max, y_max, res);
    enum_rect_recurse(node->right, x_min, x_max, y_max, res);
}

/**
 * enumerate_rectangle - Find all (x,y) with x in [x_min,x_max] and y <= y_max.
 *
 * @param root         PST root
 * @param x_min        lower bound for x
 * @param x_max        upper bound for x
 * @param y_max        maximum y value (inclusive)
 * @param pairs_found  output: number of pairs returned
 * @return             dynamically allocated array of matching Pairs (caller frees)
 */
Pair *enumerate_rectangle(Node *root, int x_min, int x_max, int y_max,
                           int *pairs_found) {
    *pairs_found = 0;
    if (!root) return NULL;

    ResultArr res = {NULL, 0, 0};
    enum_rect_recurse(root, x_min, x_max, y_max, &res);
    *pairs_found = res.cnt;
    return res.data; /* may be NULL if cnt==0 */
}

/* ─────────────────────────────────────────────
 * angular range query
 * ───────────────────────────────────────────── 
 */

/**
 * @param root         PST root
 * @param x_min        start angle (0–360)
 * @param x_max        end angle   (0–360)
 * @param y_min        minimum y value (inclusive)
 * @param pairs_found  output: number of pairs returned
 * @return             dynamically allocated array of matching Pairs (caller frees)
 **/
static void special_recurse(Node *node, int x_min, int x_max, int y_min,
                             ResultArr *res) {
    if (!node) return;
    if (node->x_max < x_min || node->x_min > x_max) return;
    if (node->p.y >= y_min &&
        node->p.x >= x_min && node->p.x <= x_max) {
        result_push(res, node->p);
    }
    special_recurse(node->left,  x_min, x_max, y_min, res);
    special_recurse(node->right, x_min, x_max, y_min, res);
}

Pair *special_query(Node *root, int x_min, int x_max, int y_min,
                    int *pairs_found) {
    *pairs_found = 0;
    if (!root) return NULL;

    ResultArr res = {NULL, 0, 0};

    if (x_min <= x_max) {
        special_recurse(root, x_min, x_max, y_min, &res);
    } else {
        /* [x_min, 360] ∪ [0, x_max] */
        special_recurse(root, x_min, 360, y_min, &res);
        special_recurse(root, 0,     x_max, y_min, &res);
    }

    *pairs_found = res.cnt;
    return res.data;
}

/* ─────────────────────────────────────────────
 * Testing 
 * ───────────────────────────────────────────── 
 */

/* Brute force */
static void bf_enum_rect(Pair *pairs, int n, int x_min, int x_max, int y_max,
                          Pair **out, int *cnt) {
    *out  = (Pair *)malloc(n * sizeof(Pair));
    *cnt  = 0;
    for (int i = 0; i < n; i++)
        if (pairs[i].x >= x_min && pairs[i].x <= x_max && pairs[i].y <= y_max)
            (*out)[(*cnt)++] = pairs[i];
}

static void bf_special(Pair *pairs, int n, int x_min, int x_max, int y_min,
                        Pair **out, int *cnt) {
    *out  = (Pair *)malloc(n * sizeof(Pair));
    *cnt  = 0;
    for (int i = 0; i < n; i++) {
        int in_range;
        if (x_min <= x_max)
            in_range = (pairs[i].x >= x_min && pairs[i].x <= x_max);
        else
            in_range = (pairs[i].x >= x_min || pairs[i].x <= x_max);
        if (in_range && pairs[i].y >= y_min)
            (*out)[(*cnt)++] = pairs[i];
    }
}

static int cmp_pair_both(const void *a, const void *b) {
    const Pair *pa = (const Pair *)a;
    const Pair *pb = (const Pair *)b;
    if (pa->x != pb->x) return pa->x - pb->x;
    return pa->y - pb->y;
}

static int results_equal(Pair *a, int na, Pair *b, int nb) {
    if (na != nb) return 0;
    qsort(a, na, sizeof(Pair), cmp_pair_both);
    qsort(b, nb, sizeof(Pair), cmp_pair_both);
    return memcmp(a, b, na * sizeof(Pair)) == 0;
}

/* ─────────────────────────────────────────────
 * Main : test benchmarks
 * ───────────────────────────────────────────── 
 */

int main(void) {
    srand((unsigned)time(NULL));

    printf("=== PST Test Suite ===\n\n");

    /* Basic correctness test */
    printf("--- Test 1: Basic correctness ---\n");
    Pair basic[] = {
        {10,5},{20,8},{30,3},{40,9},{50,1},{15,7},{25,4},{35,6},{45,2},{60,10}
    };
    int nb = sizeof(basic)/sizeof(basic[0]);
    Node *t1 = create_pst(basic, nb);

    int found; Pair *res;

    /* enumerate_rectangle    x in [10,40]    y <= 7 */
    res = enumerate_rectangle(t1, 10, 40, 7, &found);
    printf("enumerate_rectangle([10,40], y<=7): %d pairs\n", found);
    for (int i=0;i<found;i++) printf("  (%d,%d)\n",res[i].x,res[i].y);
    free(res);

    /* special_query    angle in [20,50]    y >= 4 */
    res = special_query(t1, 20, 50, 4, &found);
    printf("special_query(angle[20,50], y>=4): %d pairs\n", found);
    for (int i=0;i<found;i++) printf("  (%d,%d)\n",res[i].x,res[i].y);
    free(res);

    free_tree(t1);

    /* Duplicates */
    printf("\n--- Test 2: Duplicate coordinates ---\n");
    Pair dups[] = {{10,5},{10,5},{20,8},{20,8},{10,3}};
    int nd = sizeof(dups)/sizeof(dups[0]);
    Node *t2 = create_pst(dups, nd);
    res = enumerate_rectangle(t2, 10, 20, 8, &found);
    printf("enumerate_rectangle([10,20], y<=8): %d pairs (expect 5)\n", found);
    free(res);
    delete(&t2, (Pair){10,5});
    res = enumerate_rectangle(t2, 10, 20, 8, &found);
    printf("After deleting (10,5) once: %d pairs (expect 4)\n", found);
    free(res);
    free_tree(t2);

    /* Wrapping angular range */
    printf("\n--- Test 3: Angular wrap-around ---\n");
    Pair angles[] = {{350,5},{10,8},{180,3},{0,6},{359,9},{5,4}};
    int na2 = sizeof(angles)/sizeof(angles[0]);
    Node *t3 = create_pst(angles, na2);
    /* Query: angle 340-20 wrap   y >= 4 */
    res = special_query(t3, 340, 20, 4, &found);
    printf("special_query(angle[340,20 wrap], y>=4): %d pairs\n", found);
    for (int i=0;i<found;i++) printf("  (%d,%d)\n",res[i].x,res[i].y);
    free(res);
    free_tree(t3);

    /* Randomised testing */
    printf("\n--- Test 4: Random correctness (1000 trials) ---\n");
    int pass = 0, fail = 0;
    for (int trial = 0; trial < 1000; trial++) {
        int n = rand() % 50 + 5;
        Pair *pts = (Pair *)malloc(n * sizeof(Pair));
        for (int i = 0; i < n; i++) {
            pts[i].x = rand() % 361;
            pts[i].y = rand() % 100;
        }
        Node *tr = create_pst(pts, n);

        int x1 = rand() % 361, x2 = rand() % 361;
        if (x1 > x2) { int t = x1; x1 = x2; x2 = t; }
        int ym = rand() % 100;

        int pf; Pair *pr = enumerate_rectangle(tr, x1, x2, ym, &pf);
        int bf_cnt; Pair *bf;
        bf_enum_rect(pts, n, x1, x2, ym, &bf, &bf_cnt);

        if (results_equal(pr, pf, bf, bf_cnt)) pass++; else fail++;
        free(pr); free(bf);

        /* special_query test */
        int a1 = rand() % 361, a2 = rand() % 361;
        int ymi = rand() % 100;
        pr = special_query(tr, a1, a2, ymi, &pf);
        bf_special(pts, n, a1, a2, ymi, &bf, &bf_cnt);
        if (results_equal(pr, pf, bf, bf_cnt)) pass++; else fail++;
        free(pr); free(bf);

        free_tree(tr); free(pts);
    }
    printf("Passed: %d / %d\n", pass, pass+fail);

    /* Scaling timing */
    printf("\n--- Test 5: Scaling timing ---\n");
    int sizes[] = {1000, 5000, 10000, 50000, 100000};
    int ns = sizeof(sizes)/sizeof(sizes[0]);
    printf("%-10s %-15s %-15s %-15s %-15s\n",
           "n", "build(ms)", "query(ms)", "delete(ms)", "special(ms)");
    for (int si = 0; si < ns; si++) {
        int n = sizes[si];
        Pair *pts = (Pair *)malloc(n * sizeof(Pair));
        for (int i = 0; i < n; i++) {
            pts[i].x = rand() % 361;
            pts[i].y = rand() % 1000000;
        }

        clock_t t0 = clock();
        Node *tr = create_pst(pts, n);
        clock_t t1 = clock();
        double build_ms = 1000.0*(t1-t0)/CLOCKS_PER_SEC;

        t0 = clock();
        for (int q = 0; q < 100; q++) {
            int x1=rand()%361, x2=rand()%361, ym=rand()%1000000;
            if(x1>x2){int tmp=x1;x1=x2;x2=tmp;}
            res=enumerate_rectangle(tr,x1,x2,ym,&found);
            free(res);
        }
        t1 = clock();
        double query_ms = 1000.0*(t1-t0)/CLOCKS_PER_SEC/100.0;

        t0 = clock();
        for (int d = 0; d < 10; d++) {
            int idx = rand()%n;
            delete(&tr, pts[idx]);
        }
        t1 = clock();
        double del_ms = 1000.0*(t1-t0)/CLOCKS_PER_SEC/10.0;

        t0 = clock();
        for (int q = 0; q < 100; q++) {
            int a1=rand()%361, a2=rand()%361, ymi=rand()%1000000;
            res=special_query(tr,a1,a2,ymi,&found);
            free(res);
        }
        t1 = clock();
        double special_ms = 1000.0*(t1-t0)/CLOCKS_PER_SEC/100.0;

        printf("%-10d %-15.3f %-15.4f %-15.4f %-15.4f\n",
               n, build_ms, query_ms, del_ms, special_ms);

        free_tree(tr);
        free(pts);
    }

    printf("\n=== tests complete ===\n");
    return 0;
}
