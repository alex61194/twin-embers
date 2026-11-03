# event_scripts en español: diseño (02/11/2026)

`event_scripts.bin` (421.686 B, USA 0x15f9b4) es una sola unidad de la receta que mezcla
bytecode de scripts con textos `.string`. rex_objects.py lo convierte en NOBITS: del objeto
solo cuentan **el tamaño, los offsets de sus etiquetas y los sitios de puntero** (relocs).

Plan:
1. De la build USA (objeto `asm/event_scripts.o`): lista de símbolos (nombre, offset) y
   relocs (offset del sitio, símbolo destino + addend). Ya los recoge rex_objects.py.
2. En el PC de Alex, con las dos ROM: cada reloc de la build USA está en la ROM USA en
   0x15f9b4+offset. Llevar ese sitio a la ROM ES con anclas locales (el bytecode es igual,
   solo se desplaza entre textos), leer el puntero ES y obtener la posición ES de cada
   etiqueta referenciada. Inicio y fin ES del bloque, por los vecinos.
3. Generar un `event_scripts_es.s` sintético: `.space` + etiquetas en sus offsets ES +
   `.4byte etiqueta` en cada sitio de puntero ES. Mismo tamaño que el bloque ES. Nada de bytes de la ROM.
4. La receta ES copia el bloque ES entero con una op C.
Mismo método sirve para otros objetos asm con texto (mystery_event_msg).

## Primera prueba (es_events.py, 02/11/2026)
- Objeto USA sacado de pret (bootstrap + make tools + `make build/firered/data/event_scripts.o`): 9.832 etiquetas, 9.954 relocs ABS32, en event_scripts_layout_us.json.
- Inicio ES = USA + 140. Tamaño ES por recorrido secuencial: 420.310 (USA 421.686), pero el recorrido se desvía en 8542 (SilphCo_7F_EventScript_LaprasGuy): el script ES tiene 3 bytes más (`28 3c 00`). Es decir, **algunos scripts ES no miden lo mismo** que los USA.
- Siguiente: desensamblar el bytecode de scripts (tamaños de comando de asm/macros/event.inc de pret) en la ROM ES siguiendo punteros desde las etiquetas, en vez de suponer tamaños fijos. Así salen también los sitios de puntero ES exactos. Texto = hasta 0xFF; movimiento = hasta 0xFE.

## Resultado es_events2.py (02/11/2026 22:20)
- Desensamblador scrdis.py + script_ops.json (tamaños de 209 opcodes generados ensamblando cada macro de pret event.inc; trainerbattle por tipo).
- Recorrido ES: 3.524 scripts iguales, 12 distintos alineados por secuencia de opcodes (p. ej. ReceiveLaprasParty con un `delay` extra, Snorlax, rematch), 4.625 textos, 402 movimientos, 1.264 tablas/datos fijos.
- Tamaño ES 420.310 (USA 421.686). 9.975 sitios de puntero; 9.288 leen un puntero ES válido a una etiqueta mapeada; los 29 "raros" son la tabla gSpecialVars (punteros a RAM, normales). Queda revisar ViridianCity_Mart_Items (10 bytes distintos).
- Salida: Downloads\twinembers-es\event_scripts_layout_es.json (offsets y tamaños, sin bytes).
