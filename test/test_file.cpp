int main() {
    const int n = 10;

    // Single for loop
    int arr1[n]; 
    for (int i = 0; i < n; i++){
        arr1[i] = n - i;
    }
    
    // Single for loop, decrementing
    for (int i = n - 1; i >= 0; --i) {
        arr1[i] = n - i;
    }
    
    // Double for loop, math expression in condition, binary incrementing
    int arr2[n][2*n];
    for (int i = 0; i < n; i += 1){
        for (int j = 0; j < 2 * n; j = 1 + j){
            arr2[i][j] = (n - i) * (2 * n - j);
        }
    }

// This segfaults because omega can't parse the resulting set
/*    // Double for loop, binary incrementing, negative increments
    for (int i = 0; i < n; i -= -1) {
        for (int j = 2 * n - 1; j >= 0; j = -2 + j) {
            arr2[i][j] = n - i;
        }
    }*/
   
    // Double for loop with dependence
    int arr3[n][n];
    for (int i = 0; i < n; ++i){
        for (int j = i; j < n; ++j){
            arr2[i][j] = (n - i) * (n - j);
        }
    }

// This segfaults because omega can't parse the resulting set
/*    // Double for loop, dependence and substitution
    for (int i = 0; i < n; i += 2) {
        for (int j = 2 * i - 1; j >= i; --j) {
            arr2[i][j] = 1;
        }
    }*/

    // Imperfectly Nested for loop
    int sum = 0;
    for (int i = 0; i < n; ++i){
        sum = sum + i;
        for (int j = 0; j < n; ++j){
            arr3[i][j] = sum * j;
        } 
    }

    return 0;
}
