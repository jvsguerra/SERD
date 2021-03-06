#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <omp.h>

/******* sincos ******
* sincos[0] = sin a  *
* sincos[1] = cos a  *
* sincos[2] = sin b  *
* sincos[3] = cos b  *
*********************/

/* Grid initialization */

/*
 * Function: igrid
 * ---------------
 * 
 * Fill integer grid with 1
 * 
 * grid: empty 3D grid
 * size: number of voxels
 * 
 */
void igrid(int *grid, int size)
{
    int i;

    for (i = 0; i < size; i++)
        grid[i] = 1;
}

/* Grid filling */

/*
 * Function: fill
 * --------------
 * 
 * Insert atoms with a probe addition inside a 3D grid
 * 
 * grid: 3D grid
 * nx: x grid units
 * ny: y grid units
 * nz: z grid units
 * atoms: xyz coordinates and radii of input pdb
 * natoms: number of atoms
 * xyzr: number of data per atom (4: xyzr)
 * reference: xyz coordinates of 3D grid origin
 * ndims: number of coordinates (3: xyz)
 * sincos: sin and cos of 3D grid angles
 * nvalues: number of sin and cos (sina, cosa, sinb, cosb)
 * step: 3D grid spacing (A)
 * probe: Probe size (A)
 * nthreads: number of threads for OpenMP
 * 
 */
void fill(int *grid, int nx, int ny, int nz, double *atoms, int natoms, int xyzr, double *reference, int ndims, double *sincos, int nvalues, double step, double probe, int nthreads)
{
    int i, j, k, atom;
    double x, y, z, xaux, yaux, zaux, distance, H;

    // Set number of processes in OpenMP
    omp_set_num_threads(nthreads);
    omp_set_nested(1);

#pragma omp parallel default(none), shared(grid, reference, step, probe, natoms, nx, ny, nz, sincos, atoms, nthreads), private(atom, i, j, k, distance, H, x, y, z, xaux, yaux, zaux)
    {
#pragma omp for schedule(dynamic)
        for (atom = 0; atom < natoms; atom++)
        {
            // Convert atom coordinates in 3D grid coordinates
            x = (atoms[atom * 4] - reference[0]) / step;
            y = (atoms[1 + (atom * 4)] - reference[1]) / step;
            z = (atoms[2 + (atom * 4)] - reference[2]) / step;

            xaux = x * sincos[3] + z * sincos[2];
            yaux = y;
            zaux = (-x) * sincos[2] + z * sincos[3];

            x = xaux;
            y = yaux * sincos[1] - zaux * sincos[0];
            z = yaux * sincos[0] + zaux * sincos[1];

            // Create a radius (H) for space occupied by probe and atom
            H = (probe + atoms[3 + (atom * 4)]) / step;

            // Loop around radius from atom center
            for (i = floor(x - H); i <= ceil(x + H); i++)
                for (j = floor(y - H); j <= ceil(y + H); j++)
                    for (k = floor(z - H); k <= ceil(z + H); k++)
                    {
                        // Get distance between atom center and point inspected
                        distance = sqrt(pow(i - x, 2) + pow(j - y, 2) + pow(k - z, 2));
                        if (distance < H)
                            if (i >= 0 && i < nx && j >= 0 && j < ny && k >= 0 && k < nz)
                                grid[k + nz * (j + (ny * i))] = 0;
                    }
        }
    }
}

/* Biomolecular surface representation */

/*
 * Function: check_protein_neighbours
 * ----------------------------------
 * 
 * Checks if a cavity point on the grid is next to a protein point (0 or -2)
 * 
 * grid: 3D grid
 * dx: x grid units
 * dy: y grid units
 * dz: z grid units
 * i: x coordinate of cavity point
 * j: y coordinate of cavity point
 * k: z coordinate of cavity point
 * 
 * returns: true (int 1) or false (int 0)
 */
int check_protein_neighbours(int *grid, int nx, int ny, int nz, int i, int j, int k)
{
    int x, y, z;

    // Loop around neighboring points
    for (x = i - 1; x <= i + 1; x++)
        for (y = j - 1; y <= j + 1; y++)
            for (z = k - 1; z <= k + 1; z++)
            {
                // Check if point is inside 3D grid
                if (x < 0 || y < 0 || z < 0 || x > nx - 1 || y > ny - 1 || z > nz - 1)
                    ;
                else if (grid[z + nz * (y + (ny * x))] == 0 || grid[z + nz * (y + (ny * x))] == -2)
                    return 1;
            }
    return 0;
}

/*
 * Function: ses
 * --------------
 * 
 * Adjust surface representation to Solvent Excluded Surface (SES)
 * 
 * grid: 3D grid
 * nx: x grid units
 * ny: y grid units
 * nz: z grid units
 * step: 3D grid spacing (A)
 * probe: Probe size (A)
 * nthreads: number of threads for OpenMP
 * 
 */
void ses(int *grid, int nx, int ny, int nz, double step, double probe, int nthreads)
{
    int i, j, k, i2, j2, k2, aux;
    double distance;

    // Calculate sas limit in 3D grid units
    aux = ceil(probe / step);

    // Set number of processes in OpenMP
    omp_set_num_threads(nthreads);
    omp_set_nested(1);

#pragma omp parallel default(none), shared(grid, step, probe, aux, nx, ny, nz), private(i, j, k, i2, j2, k2, distance)
    {
#pragma omp for schedule(dynamic) collapse(3)
        // Loop around 3D grid
        for (i = 0; i < nx; i++)
            for (j = 0; j < ny; j++)
                for (k = 0; k < nz; k++)
                {
                    // Check if a cavity point
                    if (grid[k + nz * (j + (ny * i))] == 1)
                        if (check_protein_neighbours(grid, nx, ny, nz, i, j, k))
                        {
                            // Loop around sas limit from cavity point next to protein point
                            for (i2 = i - aux; i2 <= i + aux; i2++)
                                for (j2 = j - aux; j2 <= j + aux; j2++)
                                    for (k2 = k - aux; k2 <= k + aux; k2++)
                                    {
                                        if (i2 > 0 && j2 > 0 && k2 > 0 && i2 < nx && j2 < ny && k2 < nz)
                                        {
                                            // Get distance between point inspected and cavity point
                                            distance = sqrt(pow(i - i2, 2) + pow(j - j2, 2) + pow(k - k2, 2));
                                            // Check if inspected point is inside sas limit
                                            if (distance < (probe / step))
                                                if (grid[k2 + nz * (j2 + (ny * i2))] == 0)
                                                    // Mark cavity point
                                                    grid[k2 + nz * (j2 + (ny * i2))] = -2;
                                        }
                                    }
                        }
                }

#pragma omp for collapse(3)
        // Loop around 3D grid
        for (i = 0; i < nx; i++)
            for (j = 0; j < ny; j++)
                for (k = 0; k < nz; k++)
                {
                    // Mark space occupied by sas limit from protein surface
                    if (grid[k + nz * (j + (ny * i))] == -2)
                        grid[k + nz * (j + (ny * i))] = 1;
                }
    }
}

/* Surface points detection */

/*
 * Function: define_surface_points
 * -------------------------------
 * 
 * Identify surface points based on neighboring points
 * 
 * grid: 3D grid
 * nx: x grid units
 * ny: y grid units
 * nz: z grid units
 * i: x coordinate of point
 * j: y coordinate of point
 * k: z coordinate of point
 * 
 * returns: surface point (1) or solvent point (-1)
 */
int define_surface_points(int *grid, int nx, int ny, int nz, int i, int j, int k)
{
    int x, y, z;

    // Loop around neighboring points
    for (x = i - 1; x <= i + 1; x++)
        for (y = j - 1; y <= j + 1; y++)
            for (z = k - 1; z <= k + 1; z++)
            {
                // Check if point is inside 3D grid
                if (x < 0 || y < 0 || z < 0 || x > nx - 1 || y > ny - 1 || z > nz - 1)
                    ;
                else if (grid[z + nz * (y + (ny * x))] == 0)
                    return 1;
            }

    return -1;
}

/*
 * Function: filter_surface
 * ------------------------
 * 
 * Inspect 3D grid and mark detected surface points on a surface 3D grid
 * 
 * grid: 3D grid
 * surface: surface points 3D grid
 * nx: x grid units
 * ny: y grid units
 * nz: z grid units
 * nthreads: number of threads for OpenMP
 * 
 */
void filter_surface(int *grid, int nx, int ny, int nz, int nthreads)
{
    int i, j, k;

    // Set number of threads in OpenMP
    omp_set_num_threads(nthreads);
    omp_set_nested(1);

#pragma omp parallel default(none), shared(grid, nx, ny, nz), private(i, j, k)
    {
#pragma omp for collapse(3) schedule(static)
        for (i = 0; i < nx; i++)
            for (j = 0; j < ny; j++)
                for (k = 0; k < nz; k++)
                    if (grid[k + nz * (j + (ny * i))] == 1)
                        // Define surface cavity points
                        grid[k + nz * (j + (ny * i))] = define_surface_points(grid, nx, ny, nz, i, j, k);
    }
}

/* Enclosed points removal */

/*
 * Function: remove_noise_points
 * -----------------------------
 * 
 * Identify enclosed surface points based on neighboring points
 * 
 * grid: 3D grid
 * nx: x grid units
 * ny: y grid units
 * nz: z grid units
 * i: x coordinate of point
 * j: y coordinate of point
 * k: z coordinate of point
 * 
 * returns: surface point (1) or solvent point (-1)
 */
int remove_noise_points(int *grid, int nx, int ny, int nz, int i, int j, int k)
{
    int x, y, z;

    // Loop around neighboring points
    for (x = i - 1; x <= i + 1; x++)
        for (y = j - 1; y <= j + 1; y++)
            for (z = k - 1; z <= k + 1; z++)
            {
                // Check if point is inside 3D grid
                if (x < 0 || y < 0 || z < 0 || x > nx - 1 || y > ny - 1 || z > nz - 1)
                    ;
                else if (grid[z + nz * (y + (ny * x))] == -1)
                    return 1;
            }

    return 0;
}

/*
 * Function: filter_noise_points
 * -----------------------------
 * 
 * Inspect 3D grid and remove enclosed points on a surface 3D grid
 * 
 * grid: 3D grid
 * surface: surface points 3D grid
 * nx: x grid units
 * ny: y grid units
 * nz: z grid units
 * nthreads: number of threads for OpenMP
 * 
 */
void filter_noise_points(int *grid, int nx, int ny, int nz, int nthreads)
{
    int i, j, k;

    // Set number of threads in OpenMP
    omp_set_num_threads(nthreads);
    omp_set_nested(1);

#pragma omp parallel default(none), shared(grid, nx, ny, nz), private(i, j, k)
    {
#pragma omp for collapse(3) schedule(static)
        for (i = 0; i < nx; i++)
            for (j = 0; j < ny; j++)
                for (k = 0; k < nz; k++)
                    if (grid[k + nz * (j + (ny * i))] == 1)
                        // Remove enclosed regions
                        grid[k + nz * (j + (ny * i))] = remove_noise_points(grid, nx, ny, nz, i, j, k);
    }
}

/* Enclosed points removal - flood and fill algorithm */

/*
 * Variable: points
 * ----------------
 * 
 * Accumulate number of points while clustering surface points
 *
 */
int points;

/*
 * Variable: big
 * 
 * Flag that marks big surfaces on clustering
 * 
 */
int big;

/*
 * Function: check_unclustered_neighbours
 * --------------------------------------
 * 
 * Checks if a surface point on the grid is next to a unclustered surface point (1)
 * 
 * grid: 3D grid
 * dx: x grid units
 * dy: y grid units
 * dz: z grid units
 * i: x coordinate of point
 * j: y coordinate of point
 * k: z coordinate of point
 * 
 * returns: true (int 1) or false (int 0)
 */
int check_unclustered_neighbours(int *grid, int nx, int ny, int nz, int i, int j, int k)
{
    int x, y, z;

    // Loop around neighboring points
    for (x = i - 1; x <= i + 1; x++)
        for (y = j - 1; y <= j + 1; y++)
            for (z = k - 1; z <= k + 1; z++)
            {
                // Check if point is inside 3D grid
                if (x < 0 || y < 0 || z < 0 || x > nx - 1 || y > ny - 1 || z > nz - 1)
                    ;
                else if (grid[z + nz * (y + (ny * x))] > 1)
                    return grid[z + nz * (y + (ny * x))];
            }
    return 0;
}

/*
 * Function: flood_and_fill
 * ------------------------
 * 
 * Recursive flood and fill algorithm
 * 
 * grid: surface 3D grid
 * nx: x grid units
 * ny: y grid units
 * nz: z grid units
 * i: x coordinate of point
 * j: y coordinate of point
 * k: z coordinate of point
 * tag: integer identifier
 * 
 */
void flood_and_fill(int *grid, int nx, int ny, int nz, int i, int j, int k, int tag)
{
    int x, y, z;

    if (i == 0 || i == nx - 1 || j == 0 || j == ny - 1 || k == 0 || k == nz - 1)
        return;

    if (grid[k + nz * (j + (ny * i))] == 1 && !big)
    {
        grid[k + nz * (j + (ny * i))] = tag;
        points++;

        if (points == 10000)
            big = 1;

        if (!big)
        {
            for (x = i - 1; x <= i + 1; x++)
                for (y = j - 1; y <= j + 1; y++)
                    for (z = k - 1; z <= k + 1; z++)
                        flood_and_fill(grid, nx, ny, nz, x, y, z, tag);
        }
    }
}

/*
 * Function: filter_enclosed_regions
 * ---------------------------------
 * 
 * Cluster consecutive surface points together and remove enclosed surface points
 * 
 * grid: surface 3D grid
 * nx: x grid units
 * ny: y grid units
 * nz: z grid units
 * step: 3D grid spacing (A)
 * nthreads: number of threads for OpenMP
 * 
 */
void filter_enclosed_regions(int *grid, int nx, int ny, int nz, double step, int nthreads)
{
    int i, j, k, i2, j2, k2, tag, aux;

    // Set number of threads in OpenMP
    omp_set_num_threads(nthreads);
    omp_set_nested(1);

    // Initialize variables
    tag = 1;
    aux = 0;
    big = 0;

    for (i = 0; i < nx; i++)
        for (j = 0; j < ny; j++)
            for (k = 0; k < nz; k++)
                if (grid[k + nz * (j + (ny * i))] == 1)
                {
                    tag++;
                    points = 0;

                    // Clustering procedure
                    flood_and_fill(grid, nx, ny, nz, i, j, k, tag);
                    aux = points;

                    // Loop for big cavities
                    while (big)
                    {
                        aux = 0;

                        for (i2 = 0; i2 < nx; i2++)
                            for (j2 = 0; j2 < ny; j2++)
                                for (k2 = 0; k2 < nz; k2++)
                                {
                                    big = 0;
                                    aux += points;
                                    points = 0;
                                    if (grid[k2 + nz * (j2 + (ny * i2))] == 1 && check_unclustered_neighbours(grid, nx, ny, nz, i2, j2, k2) == tag)
                                        flood_and_fill(grid, nx, ny, nz, i2, j2, k2, tag);
                                }
                    }
                    points = aux;
                }

    // Convert tags
    // * 2 -> 1
    // * >2 -> -1
    if (tag > 1)
    {
#pragma omp parallel default(none), shared(grid, nx, ny, nz), private(i, j, k)
        {
#pragma omp for collapse(3) schedule(static)
            for (i = 0; i < nx; i++)
                for (j = 0; j < ny; j++)
                    for (k = 0; k < nz; k++)
                    {
                        if (grid[k + nz * (j + (ny * i))] == 2)
                            grid[k + nz * (j + (ny * i))] = 1;
                        else if (grid[k + nz * (j + (ny * i))] > 2)
                            grid[k + nz * (j + (ny * i))] = 0;
                    }
        }
    }
}

/*
 * Function: _surface
 * ------------------
 * 
 * Define solvent-exposed surface from a target biomolecule
 * 
 * grid: surface 3D grid
 * nx: x grid units
 * ny: y grid units
 * nz: z grid units
 * atoms: xyz coordinates and radii of input pdb
 * natoms: number of atoms
 * xyzr: number of data per atom (4: xyzr)
 * reference: xyz coordinates of 3D grid origin
 * ndims: number of coordinates (3: xyz)
 * sincos: sin and cos of 3D grid angles
 * nvalues: number of sin and cos (sina, cosa, sinb, cosb)
 * step: 3D grid spacing (A)
 * probe: Probe size (A)
 * is_ses: surface mode (1: SES/VDW or 0: SAS)
 * nthreads: number of threads for OpenMP
 * verbose: print extra information to standard output
 * 
 */
void _surface(int *grid, int size, int nx, int ny, int nz, double *atoms, int natoms, int xyzr, double *reference, int ndims, double *sincos, int nvalues, double step, double probe, int is_ses, int nthreads, int verbose)
{

    if (verbose)
        if (!is_ses)
            fprintf(stdout, "> Adjusting SAS surface\n");
    igrid(grid, size);
    fill(grid, nx, ny, nz, atoms, natoms, xyzr, reference, ndims, sincos, nvalues, step, probe, nthreads);

    if (is_ses)
    {
        if (verbose)
            fprintf(stdout, "> Adjusting SES surface\n");
        ses(grid, nx, ny, nz, step, probe, nthreads);
    }

    if (verbose)
        fprintf(stdout, "> Defining surface points\n");
    filter_surface(grid, nx, ny, nz, nthreads);

    if (verbose)
        fprintf(stdout, "> Filtering enclosed regions\n");
    filter_enclosed_regions(grid, nx, ny, nz, step, nthreads);
    filter_noise_points(grid, nx, ny, nz, nthreads);
}

/* Solvent-exposed residues detection */

/*
 * Struct: node
 * ------------
 * 
 * A linked list node for atom index in xyzr array
 * 
 * pos: atom index in xyzr array (coordinates and radii of pdb)
 * struct node* next: pointer to next linked list node
 *  
 */
typedef struct node
{
    int pos;
    struct node *next;
} res;

/*
 * Function: create
 * ----------------
 * 
 * Create a res node
 * 
 * pos: atom index in xyzr array (coordinates and radii of pdb)
 * 
 * returns: res node with atom index
 */
res *create(int pos)
{
    res *new = (res *)malloc(sizeof(res));

    new->pos = pos;
    new->next = NULL;

    return new;
}

/*
 * Function: insert
 * ----------------
 * 
 * Insert res node in linked list
 * 
 * res: pointer to linked list head
 * new: res node
 * 
 */
void insert(res **head, res *res_new)
{
    res *current;

    if (*head == NULL || (*head)->pos >= res_new->pos)
    {
        res_new->next = *head;
        *head = res_new;
    }
    else
    {
        current = *head;
        while (current->next != NULL && current->next->pos < res_new->pos)
        {
            current = current->next;
        }
        res_new->next = current->next;
        current->next = res_new;
    }
}

/*
 * Function: _interface
 * --------------------
 * 
 * Retrieve interface residues from solvent-exposed surface
 * 
 * grid: cavities 3D grid
 * nx: x grid units
 * ny: y grid units
 * nz: z grid units
 * pdb: 1D-array of residues information (resnum_chain)
 * atoms: xyz coordinates and radii of input pdb
 * natoms: number of atoms
 * xyzr: number of data per atom (4: xyzr)
 * reference: xyz coordinates of 3D grid origin
 * ndims: number of coordinates (3: xyz)
 * sincos: sin and cos of 3D grid angles
 * nvalues: number of sin and cos (sina, cosa, sinb, cosb)
 * step: 3D grid spacing (A)
 * probe: Probe size (A)
 * nthreads: number of threads for OpenMP
 * verbose: print information to stdout
 * 
 * returns: array of strings with interface residues
 */
char
    **
    _interface(int *grid, int nx, int ny, int nz, char **pdb, double *atoms, int natoms, int xyzr, double *reference, int ndims, double *sincos, int nvalues, double step, double probe, int nthreads, int verbose)
{
    int i, j, k, atom, count = 0, old_atom = -1;
    double x, y, z, xaux, yaux, zaux, distance, H;
    char **residues;

    if (verbose)
        fprintf(stdout, "> Retrieving interface residues\n");

    // Allocate memory for reslist structure
    res *reslist, *new_res;

    // Initialize linked list
    reslist = NULL;

    // Iterate through the list of atoms
    for (atom = 0; atom < natoms; atom++)
    {
        // Convert atom coordinates in 3D grid coordinates
        x = (atoms[atom * 4] - reference[0]) / step;
        y = (atoms[1 + (atom * 4)] - reference[1]) / step;
        z = (atoms[2 + (atom * 4)] - reference[2]) / step;

        xaux = x * sincos[3] + z * sincos[2];
        yaux = y;
        zaux = (-x) * sincos[2] + z * sincos[3];

        x = xaux;
        y = yaux * sincos[1] - zaux * sincos[0];
        z = yaux * sincos[0] + zaux * sincos[1];

        // Create a radius (H) for space occupied by probe and atom
        H = (probe + atoms[3 + (atom * 4)]) / step;

        // Loop around radius from atom center
        for (i = floor(x - H); i <= ceil(x + H); i++)
            for (j = floor(y - H); j <= ceil(y + H); j++)
                for (k = floor(z - H); k <= ceil(z + H); k++)
                {
                    if (i < nx && i > 0 && j < ny && j > 0 && k < nz && k > 0)
                        if (grid[k + nz * (j + (ny * i))] == 1)
                        {
                            distance = sqrt(pow(i - x, 2) + pow(j - y, 2) + pow(k - z, 2));
                            if (distance <= H)
                            {
                                if (old_atom != atom)
                                {
                                    new_res = create(atom);
                                    insert(&reslist, new_res);
                                    count++;
                                }
                                old_atom = atom;
                            }
                        }
                }
    }

    // Pass res information to char **
    residues = calloc(count + 1, sizeof(char *));
    new_res = reslist;
    j = 0;
    while (new_res != NULL)
    {
        residues[j++] = pdb[new_res->pos];
        new_res = new_res->next;
    }
    free(reslist);
    residues[j] = NULL;

    return residues;
}
