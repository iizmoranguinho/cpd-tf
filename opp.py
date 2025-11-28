# importacao das bibliotecas em python
import tkinter as tk # A base para criar janelas e botoes.
from tkinter import messagebox, simpledialog # Partes especificas do tkinter para mostrar caixas de mensagem.
# Importa a biblioteca de estilos e constantes
import ttkbootstrap as ttk  # A biblioteca com temas.
from ttkbootstrap.constants import *
import ctypes # o tradutor q faz a ponte d Python com o C.
import platform # Usado para descobrir se estamos rodando no Windows ou em outro sistema.
import atexit # Permite agendar uma funcao para ser executada quando o programa fecha (para limpar a memoria).
import threading  # Permite que o programa faca mais de uma coisa ao mesmo tempo (ex: carregar dados sem travar a tela).
import time # Usado para medir o tempo de carregamento dos dados.

#traduzindo as estruturas do c para py

#molde de fiome igual ta em C
class Movie(ctypes.Structure):
    pass

Movie._fields_ = [
    ("movieId", ctypes.c_int),
    ("title", ctypes.c_char_p),
    ("genres", ctypes.c_char_p * 10),
    ("genreCount", ctypes.c_int),
    ("year", ctypes.c_int),
    ("numRatings", ctypes.c_int),
    ("sumRatings", ctypes.c_double),
    ("avgRating", ctypes.c_double),
    ("next", ctypes.POINTER(Movie))
]

#molde do array de filmes pra devolver a lista de filmes
class MovieArray(ctypes.Structure):
    _fields_ = [
        ("movies", ctypes.POINTER(ctypes.POINTER(Movie))),
        ("count", ctypes.c_int),
        ("capacity", ctypes.c_int)
    ]

# array de tags
class TagArray(ctypes.Structure):
    _fields_ = [
        ("tags", ctypes.POINTER(ctypes.c_char_p)),
        ("count", ctypes.c_int),
    ]

# arq .dll
try:
    if platform.system() == "Windows":
        lib = ctypes.CDLL('./libmovie.dll')
    else:
        lib = ctypes.CDLL('./libmovie.so')
except OSError as e:
    messagebox.showerror("Erro de Biblioteca", 
        f"Nao foi possivel carregar a biblioteca C (libmovie.dll/so).\n"
        f"Verifique se voce compilou o arquivo 'movie_engine.c' corretamente.\n\nDetalhes: {e}")
    exit()

# 'argtypes' = o que a funcao recebe.
# 'restype' = o que a funcao devolve.

# func para carregar os dados.
lib.loadMovies.argtypes = [ctypes.c_char_p]
lib.loadMovies.restype = ctypes.c_int
lib.processRatings.argtypes = [ctypes.c_char_p]
lib.processRatings.restype = ctypes.c_int
lib.loadTags.argtypes = [ctypes.c_char_p]
lib.loadTags.restype = ctypes.c_int
# func de busca.
lib.getMoviesByTitlePrefix.argtypes = [ctypes.c_char_p]
lib.getMoviesByTitlePrefix.restype = ctypes.POINTER(MovieArray)
lib.getTopMoviesByGenre.argtypes = [ctypes.c_char_p, ctypes.c_int]
lib.getTopMoviesByGenre.restype = ctypes.POINTER(MovieArray)
lib.getMoviesByUser.argtypes = [ctypes.c_int]
lib.getMoviesByUser.restype = ctypes.POINTER(MovieArray)
lib.getMoviesByTags.argtypes = [ctypes.c_char_p]
lib.getMoviesByTags.restype = ctypes.POINTER(MovieArray)
lib.getTagsByMovieId.argtypes = [ctypes.c_int]
lib.getTagsByMovieId.restype = ctypes.POINTER(TagArray)
#func bonus
lib.getMoviesByYearRange.argtypes = [ctypes.c_int, ctypes.c_int]
lib.getMoviesByYearRange.restype = ctypes.POINTER(MovieArray)
# func de limpeza de memoria.
lib.freeMovieArray.argtypes = [ctypes.POINTER(MovieArray)]
lib.freeTagArray.argtypes = [ctypes.POINTER(TagArray)]
lib.freeAllMemory.argtypes = []

# interface grafica do programa
class MovieApp(ttk.Window):
    def __init__(self):
        
        super().__init__(themename="solar") #tema da interface
        
        self.title("TF CPD: Sistema de Busca de Filmes")
        self.geometry("1000x750")

        # agenda a limpeza da memoria do C para quando o programa fechar.
        atexit.register(lib.freeAllMemory)

        self.create_widgets()
        self.after(100, self.start_loading_data)

    def start_loading_data(self):
        # Cria a janelinha menor de carregando
        self.loader = ttk.Toplevel(self, "Carregando...")
        self.loader.geometry("300x120")
        self.loader.resizable(False, False)
        self.loader.transient(self)
        self.loader.grab_set()

        self.update_idletasks()

        # log para posicionar a janela de loading bem no meio da janela principal.
        x = self.winfo_x() + (self.winfo_width() // 2) - 150
        y = self.winfo_y() + (self.winfo_height() // 2) - 60
        self.loader.geometry(f"+{int(x)}+{int(y)}")

        ttk.Label(self.loader, text="Carregando dados...", font=("Segoe UI", 12), bootstyle=PRIMARY).pack(pady=(20, 10))
        pb = ttk.Progressbar(self.loader, mode='indeterminate', bootstyle=(STRIPED, SUCCESS))
        pb.pack(fill=X, padx=20)
        pb.start() #a barra de progresso comeca a se mover.

        for i in range(self.notebook.index("end")):
            self.notebook.tab(i, state="disabled")

        #o carregamento pesado roda em um thread separado
        self.loading_thread = threading.Thread(target=self._load_data_in_background)
        self.loading_thread.start()

    def _load_data_in_background(self):
        try:
            # inicia o cronometro antes de carregar os dados
            start_time = time.perf_counter()

            lib.freeAllMemory()
            lib.loadMovies(b"movies.csv")
            lib.processRatings(b"ratings.csv")
            lib.loadTags(b"tags.csv")
            
            # para o cronometro e calcula o tempo total
            end_time = time.perf_counter()
            duration = end_time - start_time
            
            # Passa o tempo calculado para a proxima funcao
            self.after(0, self._on_loading_success, duration)
        except Exception as e:
            self.after(0, self._on_loading_error, e)

    def _on_loading_success(self, duration):
        self.loader.destroy()
        for i in range(self.notebook.index("end")):
            self.notebook.tab(i, state="normal")
        
        # Mostra o tempo de carregamento na mensagem de sucesso
        messagebox.showinfo("Sucesso", f"Dados carregados com sucesso!\n\nTempo de carregamento: {duration:.2f} segundos")

    def _on_loading_error(self, error):
        self.loader.destroy()
        messagebox.showerror("Erro Critico de Carregamento", f"Ocorreu um erro ao carregar os dados:\n\n{error}")
        self.destroy()

    # funco que monta a tela principal do programa.
    def create_widgets(self):
        # cabeçalho
        header_frame = ttk.Frame(self, padding=(0, 10, 0, 20))
        header_frame.pack(fill=X)
        ttk.Label(header_frame, text="🎬 CPipocaD 🍿", font=("Segoe UI", 24, "bold"), bootstyle=PRIMARY).pack()
        ttk.Separator(header_frame).pack(fill=X, pady=10)

        #  abas de busca.
        self.notebook = ttk.Notebook(self, bootstyle=PRIMARY)
        self.notebook.pack(expand=True, fill="both", padx=20, pady=10)

        #uma aba pra cda tipo de busca
        prefix_tab = self.create_search_tab(self.notebook, [("Prefixo do Título:", "entry")], self.search_by_prefix)
        genre_tab = self.create_search_tab(self.notebook, [("Top N:", "entry"), ("Gênero:", "entry")], self.search_by_genre)
        year_range_tab = self.create_search_tab(self.notebook, [("Ano Inicial:", "entry"), ("Ano Final:", "entry")], self.search_by_year_range)
        user_tab = self.create_search_tab(self.notebook, [("ID do Usuário:", "entry")], self.search_by_user)
        tags_tab = self.create_search_tab(self.notebook, [("Tags (ex: 'sci-fi' action):", "entry")], self.search_by_tags)

        self.notebook.add(prefix_tab["frame"], text=" Por Prefixo ")
        self.notebook.add(genre_tab["frame"], text=" Top Gênero ")
        self.notebook.add(user_tab["frame"], text=" Por Usuário ")
        self.notebook.add(tags_tab["frame"], text=" Por Tags ")
        self.notebook.add(year_range_tab["frame"], text=" Por Período (Bonus) ")

    
    def create_search_tab(self, parent, fields, command):
        frame = ttk.Frame(parent, padding="20")
        
        entries = {}
        for i, (text, field_type) in enumerate(fields):
            ttk.Label(frame, text=text, font=("Segoe UI", 10)).grid(row=i, column=0, padx=5, pady=8, sticky="w")
            entry = ttk.Entry(frame, bootstyle=PRIMARY, font=("Segoe UI", 10))
            entry.grid(row=i, column=1, padx=5, pady=8, sticky="ew")
            entries[text] = entry

        search_button = ttk.Button(frame, text="Buscar", command=lambda: command(entries), bootstyle=(SUCCESS, OUTLINE))
        search_button.grid(row=len(fields), column=0, columnspan=2, pady=(20, 10))

        # A tabela onde os resultados dos filmes vao aparecer.
        tree = ttk.Treeview(frame, columns=("ID", "Título", "Gêneros", "Ano", "Nota Média", "Nº Avaliações"), show='headings', bootstyle=PRIMARY, height=15)
        for col in ("ID", "Título", "Gêneros", "Ano", "Nota Média", "Nº Avaliações"): 
            tree.heading(col, text=col)
        
        tree.column("ID", width=60, anchor=CENTER); tree.column("Título", width=350)
        tree.column("Gêneros", width=220); tree.column("Ano", width=60, anchor=CENTER)
        tree.column("Nota Média", width=90, anchor=CENTER); tree.column("Nº Avaliações", width=110, anchor=CENTER)
        
        tree.grid(row=len(fields) + 1, column=0, columnspan=2, sticky="nsew")
        frame.grid_rowconfigure(len(fields) + 1, weight=1)
        frame.grid_columnconfigure(1, weight=1)

        scrollbar = ttk.Scrollbar(frame, orient=VERTICAL, command=tree.yview, bootstyle=ROUND)
        tree.configure(yscroll=scrollbar.set)
        scrollbar.grid(row=len(fields) + 1, column=2, sticky='ns')

        tree.bind("<Double-1>", lambda event: self.show_tags_for_movie(tree))

        return {"frame": frame, "entries": entries, "tree": tree}
    
    # func que pega os dados vindos do C e preenche a tabela da interface.
    def populate_tree(self, tree, movie_array_ptr):
        for i in tree.get_children(): tree.delete(i)
        
        if not movie_array_ptr or not movie_array_ptr.contents:
            if movie_array_ptr: lib.freeMovieArray(movie_array_ptr)
            return

        results = movie_array_ptr.contents
        for i in range(results.count):
            movie = results.movies[i].contents
            genres = [movie.genres[j].decode('utf-8', errors='ignore') for j in range(movie.genreCount)]
            
            tree.insert("", "end", iid=movie.movieId, values=(
                movie.movieId, movie.title.decode('utf-8', errors='ignore'),
                " | ".join(genres), movie.year if movie.year > 0 else "N/A",
                f"{movie.avgRating:.6f}",
                movie.numRatings
            ))
        lib.freeMovieArray(movie_array_ptr)

    def search_by_prefix(self, widgets):
        prefix = widgets["Prefixo do Título:"].get()
        if prefix:
            results_ptr = lib.getMoviesByTitlePrefix(prefix.encode('utf-8'))
            self.populate_tree(self.nametowidget(widgets["Prefixo do Título:"].master).children['!treeview'], results_ptr)

    def search_by_genre(self, widgets):
        top_n_str = widgets["Top N:"].get()
        genre = widgets["Gênero:"].get()
        if top_n_str.isdigit() and genre:
            results_ptr = lib.getTopMoviesByGenre(genre.encode('utf-8'), int(top_n_str))
            self.populate_tree(self.nametowidget(widgets["Gênero:"].master).children['!treeview'], results_ptr)
            
    # A funcao que eh chamada quando o botao "Buscar" da aba de periodo eh clicado.
    def search_by_year_range(self, widgets):
        start_year_str = widgets["Ano Inicial:"].get()
        end_year_str = widgets["Ano Final:"].get()
        if start_year_str.isdigit() and end_year_str.isdigit():
            start_year = int(start_year_str)
            end_year = int(end_year_str)
            if start_year <= end_year:
                results_ptr = lib.getMoviesByYearRange(start_year, end_year)
                self.populate_tree(self.nametowidget(widgets["Ano Inicial:"].master).children['!treeview'], results_ptr)
            else:
                messagebox.showwarning("Entrada Inválida", "O ano inicial não pode ser maior que o ano final.")

    def search_by_user(self, widgets):
        user_id_str = widgets["ID do Usuário:"].get()
        if user_id_str.isdigit():
            results_ptr = lib.getMoviesByUser(int(user_id_str))
            self.populate_tree(self.nametowidget(widgets["ID do Usuário:"].master).children['!treeview'], results_ptr)

    def search_by_tags(self, widgets):
        tags_str = widgets["Tags (ex: 'sci-fi' action):"].get()
        if tags_str:
            results_ptr = lib.getMoviesByTags(tags_str.encode('utf-8'))
            self.populate_tree(self.nametowidget(widgets["Tags (ex: 'sci-fi' action):"].master).children['!treeview'], results_ptr)

    def show_tags_for_movie(self, tree):
        selected_item = tree.focus()
        if not selected_item: return
        
        movie_id = int(selected_item)
        movie_title = tree.item(selected_item)['values'][1]
        
        tags_ptr = lib.getTagsByMovieId(movie_id)
        
        tag_list = []
        if tags_ptr and tags_ptr.contents and tags_ptr.contents.count > 0:
            results = tags_ptr.contents
            for i in range(results.count):
                tag_list.append(results.tags[i].decode('utf-8', errors='ignore'))
        lib.freeTagArray(tags_ptr)
        
        tag_window = ttk.Toplevel(self)
        tag_window.title(f"Tags para: {movie_title}")
        tag_window.geometry("300x400")
        tag_window.transient(self)
        tag_window.grab_set()

        if tag_list:
            text_area = tk.Text(tag_window, wrap="word", font=("Segoe UI", 10), bg="#2c3e50", fg="white")
            text_area.pack(expand=True, fill="both", padx=10, pady=10)
            text_area.insert("1.0", "\n".join(sorted(tag_list)))
            text_area.config(state="disabled")
        else:
   
            ttk.Label(tag_window, text="Nenhuma tag encontrada para este filme.", padding=20).pack()

# A linha que efetivamente inicia o programa.
if __name__ == "__main__":
    app = MovieApp()
    app.mainloop() # Mantem a janela aberta, esperando por acoes do usuario.
