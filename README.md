# Simulador de Paginación con políticas FIFO

Este programa implementa un simulador de administración de memoria. Maneja memoria física, memoria virtual, paginación, SWAP y reemplazo de páginas mediante la política FIFO.

## Descripción general

El sistema simula cómo un sistema operativo administra procesos y páginas usando:

- RAM: marcos de página físicos.
- SWAP: área secundaria para almacenar páginas expulsadas.
- Paginación: cada proceso se divide en páginas del tamaño definido por el usuario.
- Reemplazo de página FIFO: cuando no hay marcos libres, se expulsa el marco más antiguo.

El programa usa tres hilos que actúan simultáneamente:
1. Creador de procesos: genera procesos con tamaños aleatorios y asigna sus páginas a RAM o SWAP.
2. Eliminador: borra procesos al azar liberando sus páginas.
3. Accesador: simula accesos a direcciones virtuales provocando page faults cuando es necesario.

---

## Funcionamiento del simulador

### 1. Creación de procesos

Cada proceso recibe:
- Un ID único
- Un tamaño aleatorio definido entre un mínimo y un máximo
- Un número de páginas calculado según el tamaño y el tamaño de página

Las páginas se asignan de la siguiente manera:
- Si hay marcos libres en RAM, se colocan allí.
- Si RAM está llena, las páginas van a SWAP (si hay espacio).
- Si no hay espacio en RAM ni en SWAP, la simulación se detiene.

---

### 2. Acceso a direcciones virtuales

El hilo de accesos selecciona:
- Un proceso aleatorio
- Una página aleatoria del proceso
- Un offset dentro de la página

Si la página:
- Está en RAM: acceso normal.
- No está en RAM: ocurre un page fault y se ejecuta `swap_in()`.

En `swap_in()`:
- Si hay marcos libres, se carga la página.
- Si no hay marcos libres, se aplica la política FIFO para liberar espacio.

---

### 3. Política de reemplazo FIFO

El sistema mantiene una cola circular FIFO con los marcos actualmente ocupados en RAM.

- Cada vez que una página entra a RAM, se añade al final de la cola.
- Cuando se necesita reemplazar una página, se expulsa la más antigua (primer elemento de la cola).
- La página expulsada se mueve a SWAP mediante `swap_out()`.

---

### 4. Eliminación de procesos

El hilo de eliminación selecciona periódicamente un proceso aleatorio y:

- Libera todos sus marcos en RAM.
- Libera sus páginas en SWAP.
- Elimina sus entradas de la cola FIFO.
- Ajusta la tabla de procesos para mantenerla compacta.

---

## Visualización

Después de cada operación importante, el programa imprime:

- La acción realizada(creación, eliminación, acceso) y sus datos correspondientes
- El estado de RAM (en rojo)
- El estado de SWAP (en verde)
- Las páginas presentes de cada proceso
  
## Uso e instalación 

### **Paso 1: Clonar el repositorio**  
```sh  
git clone https://github.com/gonzaamr/Tarea3SO
cd Tarea3O
```

### **Paso 2: Compila el codigo**  
Usa `gcc` para compilar:  
```sh 
gcc main.c -o tarea3 -lm
```

### **Paso 3: Ejecuta el archivo**  
```sh  
./tarea3
```

##  Requerimientos  
- Linux   
- GCC Compiler (`sudo apt install gcc` para Ubuntu/WSL)
