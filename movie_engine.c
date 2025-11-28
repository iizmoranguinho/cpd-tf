#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>

// DEFINIÇÕES GLOBAIS
#define MAX_LINE 1024       // Tamanho maximo de uma linha que vamos ler de um arquivo.
#define HASH_SIZE 1000      // Tamanho da nossa tabela hash para filmes.
#define MAX_GENRES 10       // Um filme pode ter, no maximo, 10 generos.
#define USER_HASH_SIZE 1000 // Tamanho da tabela hash para usuarios.
#define TAG_HASH_SIZE 1000  // Tamanho da tabela hash para tags.
#define MAX_QUERY_TAGS 10   // O usuario pode buscar, no maximo, 10 tags de uma vez.

// Macro para exportar funções da DLL/SO
// uma diretiva de compilador. Ela cria um rotulo especial (API_EXPORT)
// que marca quais funcoes devem ser visiveis para outros programas.
#if defined(_WIN32)
#define API_EXPORT __declspec(dllexport)
#else
#define API_EXPORT __attribute__((visibility("default")))
#endif

// ESTRUTURAS DE DADOS

// Tag. Guarda o nome da tag e uma lista de IDs de filmes associados a ela.
typedef struct TagNode
{
    char *tag;
    int *movieIds;
    int count;
    int capacity;
    struct TagNode *next;
} TagNode;

// ficha do filme
typedef struct Movie
{
    int movieId;
    char *title;
    char *genres[MAX_GENRES];
    int genreCount;
    int year;
    int numRatings;
    double sumRatings;
    double avgRating;
    struct Movie *next;
} Movie;

// avaliacao especifica de um usuario.
typedef struct UserRating
{
    int movieId;
    double rating;
    struct UserRating *next;
} UserRating;

// Usuario, Contem o ID do usuario e uma lista de todas as suas avaliacoes.
typedef struct User
{
    int userId;
    UserRating *ratings;
    struct User *next;
} User;

// Estrutura para retornar arrays de filmes
typedef struct
{
    Movie **movies;
    int count;
    int capacity;
} MovieArray;

// Estrutura para retornar arrays de tags
typedef struct
{
    char **tags;
    int count;
} TagArray;

// TABELAS HASH GLOBAIS
Movie *hashTable[HASH_SIZE] = {NULL};
User *userHashTable[USER_HASH_SIZE] = {NULL};
TagNode *tagTable[TAG_HASH_SIZE] = {NULL};

// PROTÓTIPOS DE FUNÇÕES
void appendMovieArray(MovieArray *arr, Movie *m);
Movie *searchMovieById(int movieId);

// FUNÇÕES HASH
unsigned int hash(int movieId) { return movieId % HASH_SIZE; }
unsigned int userHash(int userId) { return userId % USER_HASH_SIZE; }
unsigned int hashTag(const char *tag)
{
    unsigned long hash = 5381;
    int c;
    while ((c = (unsigned char)*tag++))
        hash = ((hash << 5) + hash) + c;
    return hash % TAG_HASH_SIZE;
}

// FUNÇÕES DE INICIALIZAÇÃO E MANIPULAÇÃO DE DADOS

// faz o array para ser preenchido com filmes
void initMovieArray(MovieArray *arr)
{
    arr->capacity = 100;
    arr->count = 0;
    arr->movies = malloc(sizeof(Movie *) * arr->capacity);
}
// Adiciona um filme ao array. Se o array encher, ele dobra de tamanho.
void appendMovieArray(MovieArray *arr, Movie *m)
{
    if (arr->count == arr->capacity)
    {
        arr->capacity *= 2;
        arr->movies = realloc(arr->movies, sizeof(Movie *) * arr->capacity);
    }
    arr->movies[arr->count++] = m;
}

// quebra a string em pedacos
void splitGenres(char *genreStr, char **genres, int *count)
{
    *count = 0;
    if (genreStr == NULL)
        return;
    char *genreCopy = strdup(genreStr);
    if (!genreCopy)
        return;

    char *token = strtok(genreCopy, "|");
    while (token && *count < MAX_GENRES)
    {
        genres[(*count)++] = strdup(token);
        token = strtok(NULL, "|");
    }
    free(genreCopy);
}

// FUNÇÕES DE CARREGAMENTO DE ARQUIVOS
void insertMovie(int movieId, const char *title, const char *genreStr, int year)
{
    unsigned int index = hash(movieId);
    Movie *newMovie = malloc(sizeof(Movie));
    newMovie->movieId = movieId;
    newMovie->title = strdup(title);
    splitGenres((char *)genreStr, newMovie->genres, &newMovie->genreCount);
    newMovie->year = year;
    newMovie->numRatings = 0;
    newMovie->sumRatings = 0.0;
    newMovie->avgRating = 0.0;
    newMovie->next = hashTable[index];
    hashTable[index] = newMovie;
}

// Funcao principal que le o arquivo movies.csv linha por linha.
API_EXPORT int loadMovies(const char *filename)
{
    FILE *file = fopen(filename, "r");
    if (!file)
        return -1;
    char line[MAX_LINE];
    int count = 0;
    fgets(line, MAX_LINE, file); // Pula a primeira linha (o cabecalho do CSV)

    while (fgets(line, MAX_LINE, file))
    {
        line[strcspn(line, "\r\n")] = 0;
        char *p = line;
        char *id_str = strchr(p, ',');
        if (!id_str)
            continue;
        *id_str = '\0';
        int movieId = atoi(p);
        p = id_str + 1;
        char *title_str, *genres_str;
        if (*p == '"')
        {
            p++;
            char *end_quote = strstr(p, "\",");
            if (!end_quote)
                continue;
            *end_quote = '\0';
            title_str = p;
            genres_str = end_quote + 2;
        }
        else
        {
            char *next_comma = strchr(p, ',');
            if (!next_comma)
                continue;
            *next_comma = '\0';
            title_str = p;
            genres_str = next_comma + 1;
        }

        int year = 0;
        char *open_paren = strrchr(title_str, '(');
        if (open_paren && open_paren > title_str)
        {
            char *end_paren = strrchr(open_paren, ')');
            if (end_paren && end_paren > open_paren)
            {
                *end_paren = '\0';
                int potential_year = atoi(open_paren + 1);
                *end_paren = ')';

                if (potential_year > 1000 && potential_year < 2030)
                {
                    year = potential_year;
                    if (*(open_paren - 1) == ' ')
                    {
                        *(open_paren - 1) = '\0';
                    }
                    else
                    {
                        *open_paren = '\0';
                    }
                }
            }
        }

        if (year == 0)
        {
            char *last_comma = strrchr(genres_str, ',');
            if (last_comma != NULL && isdigit((unsigned char)*(last_comma + 1)))
            {
                int potential_year = atoi(last_comma + 1);
                if (potential_year > 1000 && potential_year < 2030)
                {
                    year = potential_year;
                    *last_comma = '\0';
                }
            }
        }

        insertMovie(movieId, title_str, genres_str, year);
        count++;
    }
    fclose(file);
    return count;
}

// Adiciona uma avaliacao a um usuario. Se o usuario nao existir, ele eh criado.
void addUserRating(int userId, int movieId, double rating)
{
    unsigned int index = userHash(userId);
    User *user = userHashTable[index];
    while (user)
    {
        if (user->userId == userId)
            break;
        user = user->next;
    }
    if (!user)
    {
        user = malloc(sizeof(User));
        user->userId = userId;
        user->ratings = NULL;
        user->next = userHashTable[index];
        userHashTable[index] = user;
    }
    UserRating *newRating = malloc(sizeof(UserRating));
    newRating->movieId = movieId;
    newRating->rating = rating;
    newRating->next = user->ratings;
    user->ratings = newRating;
}

// Adiciona uma avaliacao a um usuario. Se o usuario nao existir, ele eh criado.
API_EXPORT int processRatings(const char *filename)
{
    FILE *file = fopen(filename, "r");
    if (!file)
        return -1;
    char line[MAX_LINE];
    int count = 0;
    fgets(line, MAX_LINE, file);
    while (fgets(line, MAX_LINE, file))
    {
        int userId, movieId;
        double rating;
        sscanf(line, "%d,%d,%lf", &userId, &movieId, &rating);
        Movie *movie = searchMovieById(movieId);
        if (movie)
        {
            movie->numRatings++;
            movie->sumRatings += rating;
        }
        addUserRating(userId, movieId, rating);
        count++;
    }
    fclose(file);
    // Depois de ler tudo, calcula a media de notas de cada filme.
    for (int i = 0; i < HASH_SIZE; i++)
    {
        Movie *curr = hashTable[i];
        while (curr)
        {
            if (curr->numRatings > 0)
                curr->avgRating = curr->sumRatings / curr->numRatings;
            curr = curr->next;
        }
    }
    return count;
}

// Adiciona uma tag e o ID do filme associado a ela na tabela hash de tags.
void insertTag(const char *tag, int movieId)
{
    char lowerTag[MAX_LINE];
    strcpy(lowerTag, tag);
    for (int i = 0; lowerTag[i]; i++)
    {
        lowerTag[i] = tolower(lowerTag[i]);
    }

    unsigned int index = hashTag(lowerTag);
    TagNode *node = tagTable[index];
    while (node)
    {
        if (strcmp(node->tag, lowerTag) == 0)
            break;
        node = node->next;
    }
    if (!node)
    {
        node = malloc(sizeof(TagNode));
        node->tag = strdup(lowerTag);
        node->count = 0;
        node->capacity = 10;
        node->movieIds = malloc(sizeof(int) * node->capacity);
        node->next = tagTable[index];
        tagTable[index] = node;
    }
    for (int i = 0; i < node->count; i++)
    {
        if (node->movieIds[i] == movieId)
            return;
    }
    if (node->count == node->capacity)
    {
        node->capacity *= 2;
        node->movieIds = realloc(node->movieIds, sizeof(int) * node->capacity);
    }
    node->movieIds[node->count++] = movieId;
}

// Le o arquivo de tags
API_EXPORT int loadTags(const char *filename)
{
    FILE *f = fopen(filename, "r");
    if (!f)
        return -1;
    char line[MAX_LINE];
    int count = 0;
    fgets(line, MAX_LINE, f);
    while (fgets(line, MAX_LINE, f))
    {
        char *p = line;
        char *first_comma = strchr(p, ',');
        if (!first_comma)
            continue;
        p = first_comma + 1;
        char *second_comma = strchr(p, ',');
        if (!second_comma)
            continue;
        *second_comma = '\0';
        int movieId = atoi(p);
        p = second_comma + 1;
        char *third_comma = strchr(p, ',');
        if (third_comma)
            *third_comma = '\0';
        else
            p[strcspn(p, "\r\n")] = 0;
        char *tag = p;
        if (strlen(tag) > 0)
        {
            if (tag[0] == '"' && tag[strlen(tag) - 1] == '"')
            {
                tag[strlen(tag) - 1] = '\0';
                tag++;
            }
            insertTag(tag, movieId);
            count++;
        }
    }
    fclose(f);
    return count;
}
//  LOGICA DE ORDENACAO
// Esta é a função que compara dois filmes.
// Ela eh usada pela função de ordenação padrão (qsort).
int compare_movies(const void *a, const void *b)
{
    Movie *m1 = *(Movie **)a;
    Movie *m2 = *(Movie **)b;

    // Critério 1: Nota média
    if (m1->avgRating > m2->avgRating)
        return -1;
    if (m1->avgRating < m2->avgRating)
        return 1;

    // Critério 2: Número de avaliações
    if (m1->numRatings > m2->numRatings)
        return -1;
    if (m1->numRatings < m2->numRatings)
        return 1;

    // Critério 3: Título em ordem alfabética (A-Z)
    return strcasecmp(m1->title, m2->title);
}

// Uma função atalho que simplesmente chama o qsort.
void sort_movie_array(MovieArray *arr)
{
    if (arr->count > 1)
    {
        qsort(arr->movies, arr->count, sizeof(Movie *), compare_movies);
    }
}

// FUNÇÕES DE BUSCA
// Estas sao as funcoes que a interface grafica pode chamar

// Busca um filme pelo ID. Funcao de apoio interna.
Movie *searchMovieById(int movieId)
{
    unsigned int index = hash(movieId);
    Movie *curr = hashTable[index];
    while (curr)
    {
        if (curr->movieId == movieId)
            return curr;
        curr = curr->next;
    }
    return NULL;
}

// As funções abaixo usam a função de ordenação `sort_movie_array`
// Pega os filmes cujo titulo comeca com o texto informado.
API_EXPORT MovieArray *getMoviesByTitlePrefix(const char *prefix)
{
    MovieArray *matched = malloc(sizeof(MovieArray));
    initMovieArray(matched);
    if (strlen(prefix) == 0)
        return matched;

    for (int i = 0; i < HASH_SIZE; i++)
    {
        Movie *curr = hashTable[i];
        while (curr)
        {
            if (strncasecmp(curr->title, prefix, strlen(prefix)) == 0)
            {
                appendMovieArray(matched, curr);
            }
            curr = curr->next;
        }
    }
    sort_movie_array(matched); // Ordena o resultado antes de devolver.
    return matched;
}

// Ordena o resultado antes de devolver.
API_EXPORT MovieArray *getMoviesByUser(int userId)
{
    MovieArray *ratedMovies = malloc(sizeof(MovieArray));
    initMovieArray(ratedMovies);

    unsigned int index = userHash(userId);
    User *user = userHashTable[index];
    while (user)
    {
        if (user->userId == userId)
            break;
        user = user->next;
    }
    if (user)
    {
        UserRating *currRating = user->ratings;
        while (currRating)
        {
            Movie *movie = searchMovieById(currRating->movieId);
            if (movie)
                appendMovieArray(ratedMovies, movie);
            currRating = currRating->next;
        }
    }
    sort_movie_array(ratedMovies);
    return ratedMovies;
}

// Pega os N melhores filmes de um genero
API_EXPORT MovieArray *getTopMoviesByGenre(const char *genre, int topN)
{
    MovieArray *filtered = malloc(sizeof(MovieArray));
    initMovieArray(filtered);

    for (int i = 0; i < HASH_SIZE; i++)
    {
        Movie *curr = hashTable[i];
        while (curr)
        {
            if (curr->numRatings > 1000)
            {
                for (int j = 0; j < curr->genreCount; j++)
                {
                    if (strcasecmp(curr->genres[j], genre) == 0)
                    {
                        appendMovieArray(filtered, curr);
                        break;
                    }
                }
            }
            curr = curr->next;
        }
    }
    // Corta a lista para devolver apenas os N primeiros
    sort_movie_array(filtered);
    if (filtered->count > topN)
    {
        filtered->count = topN;
    }
    return filtered;
}

// Pega os filmes que contem TODAS as tags informadas
API_EXPORT MovieArray *getMoviesByTags(char *tags_input)
{
    MovieArray *results = malloc(sizeof(MovieArray));
    initMovieArray(results);

    char *tags[MAX_QUERY_TAGS];
    int num_tags = 0;
    char *tags_copy = strdup(tags_input);
    char *token = tags_copy;

    while (*token && num_tags < MAX_QUERY_TAGS)
    {
        while (*token == ' ')
            token++;
        if (*token == '\0')
            break;
        char *start = token;
        if (*start == '\'')
        {
            start++;
            char *end = strchr(start, '\'');
            if (!end)
            {
                free(tags_copy);
                return results;
            }
            *end = '\0';
            token = end + 1;
        }
        else
        {
            char *end = strchr(start, ' ');
            if (end)
            {
                *end = '\0';
                token = end + 1;
            }
            else
            {
                token = start + strlen(start);
            }
        }
        for (int i = 0; start[i]; i++)
        {
            start[i] = tolower(start[i]);
        }
        tags[num_tags++] = start;
    }
    if (num_tags == 0)
    {
        free(tags_copy);
        return results;
    }

    TagNode *tag_nodes[MAX_QUERY_TAGS];
    for (int i = 0; i < num_tags; i++)
    {
        unsigned int index = hashTag(tags[i]);
        TagNode *node = tagTable[index];
        tag_nodes[i] = NULL;
        while (node)
        {
            if (strcmp(node->tag, tags[i]) == 0)
            {
                tag_nodes[i] = node;
                break;
            }
            node = node->next;
        }
        if (!tag_nodes[i])
        {
            free(tags_copy);
            return results;
        }
    }

    TagNode *first_tag_node = tag_nodes[0];
    for (int i = 0; i < first_tag_node->count; i++)
    {
        int current_movieId = first_tag_node->movieIds[i];
        int in_all_tags = 1;
        for (int j = 1; j < num_tags; j++)
        {
            int found_in_current_tag = 0;
            for (int k = 0; k < tag_nodes[j]->count; k++)
            {
                if (tag_nodes[j]->movieIds[k] == current_movieId)
                {
                    found_in_current_tag = 1;
                    break;
                }
            }
            if (!found_in_current_tag)
            {
                in_all_tags = 0;
                break;
            }
        }
        if (in_all_tags)
        {
            Movie *m = searchMovieById(current_movieId);
            if (m)
                appendMovieArray(results, m);
        }
    }
    sort_movie_array(results);
    free(tags_copy);
    return results;
}

// Pega todas as tags de um filme especifico
API_EXPORT TagArray *getTagsByMovieId(int movieId)
{
    TagArray *result = malloc(sizeof(TagArray));
    result->count = 0;
    int capacity = 10;
    result->tags = malloc(sizeof(char *) * capacity);

    for (int i = 0; i < TAG_HASH_SIZE; i++)
    {
        TagNode *node = tagTable[i];
        while (node)
        {
            for (int j = 0; j < node->count; j++)
            {
                if (node->movieIds[j] == movieId)
                {
                    if (result->count == capacity)
                    {
                        capacity *= 2;
                        result->tags = realloc(result->tags, sizeof(char *) * capacity);
                    }
                    result->tags[result->count++] = strdup(node->tag);
                    break;
                }
            }
            node = node->next;
        }
    }
    return result;
}

// Busca os filmes lancados entre um ano inicial e um final
API_EXPORT MovieArray *getMoviesByYearRange(int startYear, int endYear)
{
    MovieArray *matched = malloc(sizeof(MovieArray));
    initMovieArray(matched);

    for (int i = 0; i < HASH_SIZE; i++)
    {
        Movie *curr = hashTable[i];
        while (curr)
        {
            if (curr->year >= startYear && curr->year <= endYear)
            {
                appendMovieArray(matched, curr);
            }
            curr = curr->next;
        }
    }
    sort_movie_array(matched);
    return matched;
}

// Funções para liberar memória
// libera a memoria de um array de filmes
API_EXPORT void freeMovieArray(MovieArray *arr)
{
    if (arr)
    {
        free(arr->movies);
        free(arr);
    }
}

// liebra a memoria do array de tags
API_EXPORT void freeTagArray(TagArray *arr)
{
    if (arr)
    {
        for (int i = 0; i < arr->count; i++)
        {
            free(arr->tags[i]);
        }
        free(arr->tags);
        free(arr);
    }
}
// libera tuddo
API_EXPORT void freeAllMemory()
{
    for (int i = 0; i < HASH_SIZE; i++)
    {
        Movie *curr = hashTable[i];
        while (curr)
        {
            Movie *tmp = curr;
            curr = curr->next;
            free(tmp->title);
            for (int j = 0; j < tmp->genreCount; j++)
                free(tmp->genres[j]);
            free(tmp);
        }
        hashTable[i] = NULL;
    }
    for (int i = 0; i < USER_HASH_SIZE; i++)
    {
        User *user = userHashTable[i];
        while (user)
        {
            UserRating *rating = user->ratings;
            while (rating)
            {
                UserRating *tmpR = rating;
                rating = rating->next;
                free(tmpR);
            }
            User *tmpU = user;
            user = user->next;
            free(tmpU);
        }
        userHashTable[i] = NULL;
    }
    for (int i = 0; i < TAG_HASH_SIZE; i++)
    {
        TagNode *curr = tagTable[i];
        while (curr)
        {
            TagNode *tmp = curr;
            curr = curr->next;
            free(tmp->tag);
            free(tmp->movieIds);
            free(tmp);
        }
        tagTable[i] = NULL;
    }
}
