#include <string.h>

const char *get_system_language(void);

const char *noct_gettext(const char *msg)
{
    const char *lang_code = get_system_language();
    if (strcmp(msg, "syntax error") == 0) {
        if (strcmp(lang_code, "en") == 0) return "Syntax error.";
        if (strcmp(lang_code, "ja") == 0) return "文法エラーです。";
        if (strcmp(lang_code, "ca") == 0) return "Error de sintaxi.";
        if (strcmp(lang_code, "es") == 0) return "Error de sintaxis.";
        if (strcmp(lang_code, "de") == 0) return "Syntaxfehler.";
        if (strcmp(lang_code, "it") == 0) return "Errore di sintassi.";
        return "syntax error";
    }
    if (strcmp(msg, "%s: Out of memory while parsing.") == 0) {
        if (strcmp(lang_code, "ja") == 0) return "%s: メモリが足りません。";
        if (strcmp(lang_code, "ca") == 0) return "%s: No hi ha prou memòria.";
        if (strcmp(lang_code, "es") == 0) return "%s: No hay suficiente memoria.";
        if (strcmp(lang_code, "de") == 0) return "%s: Nicht genügend Speicher beim Parsen.";
        if (strcmp(lang_code, "it") == 0) return "%s: Non c'è abbastanza memoria durante il parsing.";
        return "%s: Out of memory while parsing.";
    }
    if (strcmp(msg, "Too many functions.") == 0) {
        if (strcmp(lang_code, "ja") == 0) return "関数が多すぎます。";
        if (strcmp(lang_code, "ca") == 0) return "Hi ha massa funcions.";
        if (strcmp(lang_code, "es") == 0) return "Hay demasiadas funciones.";
        if (strcmp(lang_code, "de") == 0) return "Zu viele Funktionen.";
        if (strcmp(lang_code, "it") == 0) return "Ci sono troppe funzioni.";
        return "Too many functions.";
    }
    if (strcmp(msg, "continue appeared outside loop.") == 0) {
        if (strcmp(lang_code, "ja") == 0) return "continue文がループの外で使用されました。";
        if (strcmp(lang_code, "ca") == 0) return "continue és utilitzat fora d'un bucle.";
        if (strcmp(lang_code, "es") == 0) return "continue es utilizado fuera de un bucle.";
        if (strcmp(lang_code, "de") == 0) return "continue wurde außerhalb einer Schleife verwendet.";
        if (strcmp(lang_code, "it") == 0) return "continue è stato utilizzato al di fuori di un ciclo.";
        return "continue appeared outside loop.";
    }
    if (strcmp(msg, "LHS is not a term or an array element.") == 0) {
        if (strcmp(lang_code, "ja") == 0) return "左辺が項か配列要素ではありません。";
        if (strcmp(lang_code, "ca") == 0) return "LHS no és un terme ni un element d'array.";
        if (strcmp(lang_code, "es") == 0) return "LHS no es un término ni un elemento de un array.";
        if (strcmp(lang_code, "de") == 0) return "LHS ist weder ein Term noch ein Array-Element.";
        if (strcmp(lang_code, "it") == 0) return "LHS non è un termine né un elemento di un array.";
        return "LHS is not a term or an array element.";
    }
    if (strcmp(msg, "var is specified without a single symbol.") == 0) {
        if (strcmp(lang_code, "ja") == 0) return "varが変数名以外に指定されました。";
        if (strcmp(lang_code, "ca") == 0) return "var és especificat sense un sol símbol.";
        if (strcmp(lang_code, "es") == 0) return "var es especificado sin un solo símbolo.";
        if (strcmp(lang_code, "de") == 0) return "var wird ohne ein einzelnes Symbol angegeben.";
        if (strcmp(lang_code, "it") == 0) return "var è specificato senza un singolo simbolo.";
        return "var is specified without a single symbol.";
    }
    if (strcmp(msg, "else-if block appeared without if block.") == 0) {
        if (strcmp(lang_code, "ja") == 0) return "else if文がifブロックの後ろ以外で使用されました。";
        if (strcmp(lang_code, "ca") == 0) return "bloc else-if és utilitzat fora el bloc if.";
        if (strcmp(lang_code, "es") == 0) return "bloque else-if utilizado sin un bloque if.";
        if (strcmp(lang_code, "de") == 0) return "else-if-Block wurde ohne zugehörigen if-Block verwendet.";
        if (strcmp(lang_code, "it") == 0) return "blocco else-if è stato utilizzato senza un blocco if.";
        return "else-if block appeared without if block.";
    }
    if (strcmp(msg, "else-if appeared after else.") == 0) {
        if (strcmp(lang_code, "ja") == 0) return "else if文がelse文の後ろで使用されました。";
        if (strcmp(lang_code, "ca") == 0) return "else-if és utilitzat després d'else.";
        if (strcmp(lang_code, "es") == 0) return "else-if utilizado después de else.";
        if (strcmp(lang_code, "de") == 0) return "else-if wurde nach else verwendet.";
        if (strcmp(lang_code, "it") == 0) return "else-if è stato utilizzato dopo else.";
        return "else-if appeared after else.";
    }
    if (strcmp(msg, "else appeared after else.") == 0) {
        if (strcmp(lang_code, "ja") == 0) return "else文がelse文の後ろで使用されました。";
        if (strcmp(lang_code, "ca") == 0) return "else és utilitzat després d'else.";
        if (strcmp(lang_code, "es") == 0) return "else utilizado después de else.";
        if (strcmp(lang_code, "de") == 0) return "else wurde nach else verwendet.";
        if (strcmp(lang_code, "it") == 0) return "else è stato utilizzato dopo else.";
        return "else appeared after else.";
    }
    if (strcmp(msg, "Exceeded the maximum argument count.") == 0) {
        if (strcmp(lang_code, "ja") == 0) return "引数が多すぎます。";
        if (strcmp(lang_code, "ca") == 0) return "S'ha superat el nombre màxim d'arguments.";
        if (strcmp(lang_code, "es") == 0) return "Se ha superado el número máximo de argumentos.";
        if (strcmp(lang_code, "de") == 0) return "Die maximale Anzahl von Argumenten wurde überschritten.";
        if (strcmp(lang_code, "it") == 0) return "È stato superato il numero massimo di argomenti.";
        return "Exceeded the maximum argument count.";
    }
    if (strcmp(msg, "Too many anonymous functions.") == 0) {
        if (strcmp(lang_code, "ja") == 0) return "無名関数が多すぎます。";
        if (strcmp(lang_code, "ca") == 0) return "Hi ha massa funcions anònimes.";
        if (strcmp(lang_code, "es") == 0) return "Hay demasiadas funciones anónimas.";
        if (strcmp(lang_code, "de") == 0) return "Zu viele anonyme Funktionen.";
        if (strcmp(lang_code, "it") == 0) return "Ci sono troppe funzioni anonime.";
        return "Too many anonymous functions.";
    }
    if (strcmp(msg, "LHS is not a symbol or an array element.") == 0) {
        if (strcmp(lang_code, "ja") == 0) return "左辺がシンボルか配列要素ではありません。";
        if (strcmp(lang_code, "ca") == 0) return "LHS no és un símbol ni un element d'array.";
        if (strcmp(lang_code, "es") == 0) return "LHS no es un símbolo ni un elemento de un array.";
        if (strcmp(lang_code, "de") == 0) return "LHS ist weder ein Symbol noch ein Array-Element.";
        if (strcmp(lang_code, "it") == 0) return "LHS non è un simbolo né un elemento di un array.";
        return "LHS is not a symbol or an array element.";
    }
    if (strcmp(msg, "Too many local variables.") == 0) {
        if (strcmp(lang_code, "ja") == 0) return "ローカル変数が多すぎます。";
        if (strcmp(lang_code, "ca") == 0) return "Hi ha massa variables locals.";
        if (strcmp(lang_code, "es") == 0) return "Hay demasiadas variables locales.";
        if (strcmp(lang_code, "de") == 0) return "Zu viele lokale Variablen.";
        if (strcmp(lang_code, "it") == 0) return "Ci sono troppe variabili locali.";
        return "Too many local variables.";
    }
    if (strcmp(msg, "Too many jumps.") == 0) {
        if (strcmp(lang_code, "ja") == 0) return "ジャンプが多すぎます。";
        if (strcmp(lang_code, "ca") == 0) return "Hi ha massa salts.";
        if (strcmp(lang_code, "es") == 0) return "Hay demasiados saltos.";
        if (strcmp(lang_code, "de") == 0) return "Zu viele Sprünge.";
        if (strcmp(lang_code, "it") == 0) return "Ci sono troppi salti.";
        return "Too many jumps.";
    }
    if (strcmp(msg, "LIR: Out of memory error.") == 0) {
        if (strcmp(lang_code, "ja") == 0) return "LIR: メモリが足りません。";
        if (strcmp(lang_code, "ca") == 0) return "LIR: No hi ha prou memòria.";
        if (strcmp(lang_code, "es") == 0) return "LIR: No hay suficiente memoria.";
        if (strcmp(lang_code, "de") == 0) return "LIR: Nicht genügend Speicher.";
        if (strcmp(lang_code, "it") == 0) return "LIR: Non c'è abbastanza memoria.";
        return "LIR: Out of memory error.";
    }
    if (strcmp(msg, "Memory mapping failed.") == 0) {
        if (strcmp(lang_code, "ja") == 0) return "メモリマップに失敗しました。";
        if (strcmp(lang_code, "ca") == 0) return "El mapatge de la memòria ha fallat.";
        if (strcmp(lang_code, "es") == 0) return "El mapeo de la memoria ha fallado.";
        if (strcmp(lang_code, "de") == 0) return "Speicherabbildung fehlgeschlagen.";
        if (strcmp(lang_code, "it") == 0) return "Mappatura della memoria fallita.";
        return "Memory mapping failed.";
    }
    if (strcmp(msg, "Code too big.") == 0) {
        if (strcmp(lang_code, "ja") == 0) return "コードが大きすぎます。";
        if (strcmp(lang_code, "ca") == 0) return "El codi és massa gran.";
        if (strcmp(lang_code, "es") == 0) return "El código es demasiado grande.";
        if (strcmp(lang_code, "de") == 0) return "Der Code ist zu groß.";
        if (strcmp(lang_code, "it") == 0) return "Il codice è troppo grande.";
        return "Code too big.";
    }
    if (strcmp(msg, "Branch target not found.") == 0) {
        if (strcmp(lang_code, "ja") == 0) return "分岐先がみつかりません。";
        if (strcmp(lang_code, "ca") == 0) return "El destí de la branca no s'ha trobat.";
        if (strcmp(lang_code, "es") == 0) return "El destino de la rama no se ha encontrado.";
        if (strcmp(lang_code, "de") == 0) return "Sprungziel nicht gefunden.";
        if (strcmp(lang_code, "it") == 0) return "Destinazione del ramo non trovata.";
        return "Branch target not found.";
    }
    if (strcmp(msg, "Failed to load bytecode.") == 0) {
        if (strcmp(lang_code, "ja") == 0) return "バイトコードの読み込みに失敗しました。";
        if (strcmp(lang_code, "ca") == 0) return "No s'ha pogut carregar el bytecode.";
        if (strcmp(lang_code, "es") == 0) return "No se ha podido cargar el bytecode.";
        if (strcmp(lang_code, "de") == 0) return "Bytecode konnte nicht geladen werden.";
        if (strcmp(lang_code, "it") == 0) return "Impossibile caricare il bytecode.";
        return "Failed to load bytecode.";
    }
    if (strcmp(msg, "Cannot find function %s.") == 0) {
        if (strcmp(lang_code, "ja") == 0) return "関数%sがみつかりません。";
        if (strcmp(lang_code, "ca") == 0) return "No s'ha trobat la funció %s.";
        if (strcmp(lang_code, "es") == 0) return "No se ha encontrado la función %s.";
        if (strcmp(lang_code, "de") == 0) return "Funktion %s nicht gefunden.";
        if (strcmp(lang_code, "it") == 0) return "Funzione %s non trovata.";
        return "Cannot find function %s.";
    }
    if (strcmp(msg, "Not an array.") == 0) {
        if (strcmp(lang_code, "ja") == 0) return "配列ではありません。";
        if (strcmp(lang_code, "ca") == 0) return "No és un array.";
        if (strcmp(lang_code, "es") == 0) return "No es un array.";
        if (strcmp(lang_code, "de") == 0) return "Kein Array.";
        if (strcmp(lang_code, "it") == 0) return "Non è un array.";
        return "Not an array.";
    }
    if (strcmp(msg, "Array index %d is out-of-range.") == 0) {
        if (strcmp(lang_code, "ja") == 0) return "配列の添字 %d は範囲外です。";
        if (strcmp(lang_code, "ca") == 0) return "L'índex %d de l'array es troba fora del rang.";
        if (strcmp(lang_code, "es") == 0) return "El índice %d del array se encuentra fuera de rango.";
        if (strcmp(lang_code, "de") == 0) return "Array-Index %d ist außerhalb des gültigen Bereichs.";
        if (strcmp(lang_code, "it") == 0) return "L'indice %d dell'array è fuori dall'intervallo.";
        return "Array index %d is out-of-range.";
    }
    if (strcmp(msg, "Dictionary key \"%s\" not found.") == 0) {
        if (strcmp(lang_code, "ja") == 0) return "辞書のキー \"%s\" がみつかりません。";
        if (strcmp(lang_code, "ca") == 0) return "La clau \"%s\" del diccionari no s'ha trobat.";
        if (strcmp(lang_code, "es") == 0) return "La llave \"%s\" no se encontró en el diccionario.";
        if (strcmp(lang_code, "de") == 0) return "Schlüssel \"%s\" im Dictionary nicht gefunden.";
        if (strcmp(lang_code, "it") == 0) return "Chiave \"%s\" non trovata nel dizionario.";
        return "Dictionary key \"%s\" not found.";
    }
    if (strcmp(msg, "Local variable \"%s\" not found.") == 0) {
        if (strcmp(lang_code, "ja") == 0) return "ローカル変数 \"%s\" がみつかりません。";
        if (strcmp(lang_code, "ca") == 0) return "La variable local \"%s\" no s'ha trobat.";
        if (strcmp(lang_code, "es") == 0) return "La variable local \"%s\" no se ha encontrado.";
        if (strcmp(lang_code, "de") == 0) return "Lokale Variable \"%s\" nicht gefunden.";
        if (strcmp(lang_code, "it") == 0) return "Variabile locale \"%s\" non trovata.";
        return "Local variable \"%s\" not found.";
    }
    if (strcmp(msg, "Global variable \"%s\" not found.") == 0) {
        if (strcmp(lang_code, "ja") == 0) return "グローバル変数 \"%s\" がみつかりません。";
        if (strcmp(lang_code, "ca") == 0) return "La variable global \"%s\" no s'ha trobat.";
        if (strcmp(lang_code, "es") == 0) return "La variable global \"%s\" no se ha encontrado.";
        if (strcmp(lang_code, "de") == 0) return "Globale Variable \"%s\" nicht gefunden.";
        if (strcmp(lang_code, "it") == 0) return "Variabile globale \"%s\" non trovata.";
        return "Global variable \"%s\" not found.";
    }
    if (strcmp(msg, "Value is not a number.") == 0) {
        if (strcmp(lang_code, "ja") == 0) return "値が数ではありません。";
        if (strcmp(lang_code, "ca") == 0) return "El valor no és un número.";
        if (strcmp(lang_code, "es") == 0) return "El valor no es un número.";
        if (strcmp(lang_code, "de") == 0) return "Wert ist keine Zahl.";
        if (strcmp(lang_code, "it") == 0) return "Il valore non è un numero.";
        return "Value is not a number.";
    }
    if (strcmp(msg, "Value is not an integer.") == 0) {
        if (strcmp(lang_code, "ja") == 0) return "値が整数ではありません。";
        if (strcmp(lang_code, "ca") == 0) return "El valor no és un enter.";
        if (strcmp(lang_code, "es") == 0) return "El valor no es un entero.";
        if (strcmp(lang_code, "de") == 0) return "Wert ist keine ganze Zahl.";
        if (strcmp(lang_code, "it") == 0) return "Il valore non è un intero.";
        return "Value is not an integer.";
    }
    if (strcmp(msg, "Value is not a string.") == 0) {
        if (strcmp(lang_code, "ja") == 0) return "値が文字列ではありません。";
        if (strcmp(lang_code, "ca") == 0) return "El valor no és un string.";
        if (strcmp(lang_code, "es") == 0) return "El valor no es un string.";
        if (strcmp(lang_code, "de") == 0) return "Wert ist kein String.";
        if (strcmp(lang_code, "it") == 0) return "Il valore non è un string.";
        return "Value is not a string.";
    }
    if (strcmp(msg, "Value is not a number or a string.") == 0) {
        if (strcmp(lang_code, "ja") == 0) return "値が数か文字列ではありません。";
        if (strcmp(lang_code, "ca") == 0) return "El valor no és un número ni un string.";
        if (strcmp(lang_code, "es") == 0) return "El valor no es un número ni un string.";
        if (strcmp(lang_code, "de") == 0) return "Wert ist weder eine Zahl noch ein String.";
        if (strcmp(lang_code, "it") == 0) return "Il valore non è né un numero né un string.";
        return "Value is not a number or a string.";
    }
    if (strcmp(msg, "Division by zero.") == 0) {
        if (strcmp(lang_code, "ja") == 0) return "ゼロによる除算です。";
        if (strcmp(lang_code, "ca") == 0) return "Divisió per zero.";
        if (strcmp(lang_code, "es") == 0) return "División por cero.";
        if (strcmp(lang_code, "de") == 0) return "Division durch Null.";
        if (strcmp(lang_code, "it") == 0) return "Divisione per zero.";
        return "Division by zero.";
    }
    if (strcmp(msg, "Subscript not an integer.") == 0) {
        if (strcmp(lang_code, "ja") == 0) return "添字が整数ではありません。";
        if (strcmp(lang_code, "ca") == 0) return "El subíndex no és un enter.";
        if (strcmp(lang_code, "es") == 0) return "El subíndice no es un entero.";
        if (strcmp(lang_code, "de") == 0) return "Der Index ist keine ganze Zahl.";
        if (strcmp(lang_code, "it") == 0) return "L'indice non è un intero.";
        return "Subscript not an integer.";
    }
    if (strcmp(msg, "Not an array or a dictionary.") == 0) {
        if (strcmp(lang_code, "ja") == 0) return "配列か辞書ではありません。";
        if (strcmp(lang_code, "ca") == 0) return "No és un array ni un diccionari.";
        if (strcmp(lang_code, "es") == 0) return "No es un array ni un diccionario.";
        if (strcmp(lang_code, "de") == 0) return "Weder ein Array noch ein Dictionary.";
        if (strcmp(lang_code, "it") == 0) return "Non è un array né un dizionario.";
        return "Not an array or a dictionary.";
    }
    if (strcmp(msg, "Subscript not a string.") == 0) {
        if (strcmp(lang_code, "ja") == 0) return "添字が文字列ではありません。";
        if (strcmp(lang_code, "ca") == 0) return "El subíndex no és un string.";
        if (strcmp(lang_code, "es") == 0) return "El subíndice no es un string.";
        if (strcmp(lang_code, "de") == 0) return "Der Index ist kein String.";
        if (strcmp(lang_code, "it") == 0) return "L'indice non è un string.";
        return "Subscript not a string.";
    }
    if (strcmp(msg, "Value is not a string, an array, or a dictionary.") == 0) {
        if (strcmp(lang_code, "ja") == 0) return "値が文字列、配列、辞書ではありません。";
        if (strcmp(lang_code, "ca") == 0) return "El valor no és un string, un array ni un diccionari.";
        if (strcmp(lang_code, "es") == 0) return "El valor no es un string, un array ni un diccionario.";
        if (strcmp(lang_code, "de") == 0) return "Wert ist weder ein String, noch ein Array noch ein Dictionary.";
        if (strcmp(lang_code, "it") == 0) return "Il valore non è né un string, né un array né un dizionario.";
        return "Value is not a string, an array, or a dictionary.";
    }
    if (strcmp(msg, "Not a dictionary.") == 0) {
        if (strcmp(lang_code, "ja") == 0) return "辞書ではありません。";
        if (strcmp(lang_code, "ca") == 0) return "No és un diccionari.";
        if (strcmp(lang_code, "es") == 0) return "No es un diccionario.";
        if (strcmp(lang_code, "de") == 0) return "Kein Dictionary.";
        if (strcmp(lang_code, "it") == 0) return "Non è un dizionario.";
        return "Not a dictionary.";
    }
    if (strcmp(msg, "Dictionary index out-of-range.") == 0) {
        if (strcmp(lang_code, "ja") == 0) return "辞書のインデックスが範囲外です。";
        if (strcmp(lang_code, "ca") == 0) return "L'índex del diccionari està fora de rang.";
        if (strcmp(lang_code, "es") == 0) return "El índice del diccionario está fuera de rango.";
        if (strcmp(lang_code, "de") == 0) return "Dictionary-Index außerhalb des gültigen Bereichs.";
        if (strcmp(lang_code, "it") == 0) return "L'indice del dizionario è fuori dall'intervallo.";
        return "Dictionary index out-of-range.";
    }
    if (strcmp(msg, "Symbol \"%s\" not found.") == 0) {
        if (strcmp(lang_code, "ja") == 0) return "シンボル \"%s\" がみつかりません。";
        if (strcmp(lang_code, "ca") == 0) return "El símbol \"%s\" no s'ha trobat.";
        if (strcmp(lang_code, "es") == 0) return "El símbolo \"%s\" no se ha encontrado.";
        if (strcmp(lang_code, "de") == 0) return "Symbol \"%s\" nicht gefunden.";
        if (strcmp(lang_code, "it") == 0) return "Simbolo \"%s\" non trovato.";
        return "Symbol \"%s\" not found.";
    }
    if (strcmp(msg, "Function arguments not match.") == 0) {
        if (strcmp(lang_code, "ja") == 0) return "関数の引数が一致しません。";
        return "Function arguments not match.";
    }
    if (strcmp(msg, "Element %d is not an integer.") == 0) {
        if (strcmp(lang_code, "ja") == 0) return "%d 番目の要素が整数ではありません。";
        return "Element %d is not an integer.";
    }
    if (strcmp(msg, "Element %d is not a float.") == 0) {
        if (strcmp(lang_code, "ja") == 0) return "%d 番目の要素が浮動小数点数ではありません。";
        return "Element %d is not a float.";
    }
    if (strcmp(msg, "Element %d is not a string.") == 0) {
        if (strcmp(lang_code, "ja") == 0) return "%d 番目の要素が文字列ではありません。";
        return "Element %d is not a string.";
    }
    if (strcmp(msg, "Element %d is not an array.") == 0) {
        if (strcmp(lang_code, "ja") == 0) return "%d 番目の要素が配列ではありません。";
        return "Element %d is not an array.";
    }
    if (strcmp(msg, "Element %d is not a dictionary.") == 0) {
        if (strcmp(lang_code, "ja") == 0) return "%d 番目の要素が辞書ではありません。";
        return "Element %d is not a dictionary.";
    }
    if (strcmp(msg, "Element %d is not a function.") == 0) {
        if (strcmp(lang_code, "ja") == 0) return "%d 番目の要素が関数ではありません。";
        return "Element %d is not a function.";
    }
    if (strcmp(msg, "Value for key %s is not an integer.") == 0) {
        if (strcmp(lang_code, "ja") == 0) return "キー %s に対応する値が整数ではありません。";
        return "Value for key %s is not an integer.";
    }
    if (strcmp(msg, "Value for key %s is not an float.") == 0) {
        if (strcmp(lang_code, "ja") == 0) return "キー %s に対応する値が浮動小数点数ではありません。";
        return "Value for key %s is not an float.";
    }
    if (strcmp(msg, "Value for key %s is not a string.") == 0) {
        if (strcmp(lang_code, "ja") == 0) return "キー %s に対応する値が文字列ではありません。";
        return "Value for key %s is not a string.";
    }
    if (strcmp(msg, "Value for key %s is not an array.") == 0) {
        if (strcmp(lang_code, "ja") == 0) return "キー %s に対応する値が配列ではありません。";
        return "Value for key %s is not an array.";
    }
    if (strcmp(msg, "Value for key %s is not a dictionary.") == 0) {
        if (strcmp(lang_code, "ja") == 0) return "キー %s に対応する値が辞書ではありません。";
        return "Value for key %s is not a dictionary.";
    }
    if (strcmp(msg, "Value for key %s is not a function.") == 0) {
        if (strcmp(lang_code, "ja") == 0) return "キー %s に対応する値が関数ではありません。";
        return "Value for key %s is not a function.";
    }
    if (strcmp(msg, "Argument (%d: %s) not an integer.\n") == 0) {
        if (strcmp(lang_code, "ja") == 0) return "引数 (%d: %s) が整数ではありません。\n";
        return "Argument (%d: %s) not an integer.\n";
    }
    if (strcmp(msg, "Argument (%d: %s) not a float.\n") == 0) {
        if (strcmp(lang_code, "ja") == 0) return "引数 (%d: %s) が浮動小数点数ではありません。\n";
        return "Argument (%d: %s) not a float.\n";
    }
    if (strcmp(msg, "Argument (%d: %s) not a string.\n") == 0) {
        if (strcmp(lang_code, "ja") == 0) return "引数 (%d: %s) が文字列ではありません。\n";
        return "Argument (%d: %s) not a string.\n";
    }
    if (strcmp(msg, "Argument (%d: %s) is not an array.\n") == 0) {
        if (strcmp(lang_code, "ja") == 0) return "引数 (%d: %s) が配列ではありません。\n";
        return "Argument (%d: %s) is not an array.\n";
    }
    if (strcmp(msg, "Argument (%d: %s) is not a dictionary.\n") == 0) {
        if (strcmp(lang_code, "ja") == 0) return "引数 (%d: %s) が辞書ではありません。\n";
        return "Argument (%d: %s) is not a dictionary.\n";
    }
    if (strcmp(msg, "Not a function.") == 0) {
        if (strcmp(lang_code, "ja") == 0) return "関数ではありません。";
        if (strcmp(lang_code, "ca") == 0) return "No és una funció.";
        if (strcmp(lang_code, "es") == 0) return "No es una función.";
        if (strcmp(lang_code, "de") == 0) return "Keine Funktion.";
        if (strcmp(lang_code, "it") == 0) return "Non è una funzione.";
        return "Not a function.";
    }
    if (strcmp(msg, "Out of memory.") == 0) {
        if (strcmp(lang_code, "ja") == 0) return "メモリが足りません。";
        if (strcmp(lang_code, "ca") == 0) return "No hi ha prou memòria.";
        if (strcmp(lang_code, "es") == 0) return "No hay suficiente memoria.";
        if (strcmp(lang_code, "de") == 0) return "Nicht genügend Speicher.";
        if (strcmp(lang_code, "it") == 0) return "Non c'è abbastanza memoria.";
        return "Out of memory.";
    }
    if (strcmp(msg, "Cannot open file %s.\n") == 0) {
        if (strcmp(lang_code, "ja") == 0) return "ファイル%sを開けません。\n";
        return "Cannot open file %s.\n";
    }
    if (strcmp(msg, "Cannon open file %s") == 0) {
        if (strcmp(lang_code, "ja") == 0) return "ファイル%sを開けません。";
        return "Cannon open file %s";
    }
    if (strcmp(msg, "Cannot read file %s.\n") == 0) {
        if (strcmp(lang_code, "ja") == 0) return "ファイル%sを読み込めません。\n";
        return "Cannot read file %s.\n";
    }
    if (strcmp(msg, "Error: %s: %d: %s\n") == 0) {
        if (strcmp(lang_code, "ja") == 0) return "エラー: %s: %d: %s\n";
        return "Error: %s: %d: %s\n";
    }
    if (strcmp(msg, "Adding file %s\n") == 0) {
        if (strcmp(lang_code, "ja") == 0) return "ファイル%sを追加しています。\n";
        return "Adding file %s\n";
    }
    if (strcmp(msg, "Cannot find %s.\n") == 0) {
        if (strcmp(lang_code, "ja") == 0) return "%sがみつかりません。\n";
        return "Cannot find %s.\n";
    }
    if (strcmp(msg, "Searching directory %s.\n") == 0) {
        if (strcmp(lang_code, "ja") == 0) return "ディレクトリ%sを検索中です。\n";
        return "Searching directory %s.\n";
    }
    if (strcmp(msg, "Skipping empty directory %s.\n") == 0) {
        if (strcmp(lang_code, "ja") == 0) return "空のディレクトリ%sをスキップしています。\n";
        return "Skipping empty directory %s.\n";
    }
    if (strcmp(msg, "Adding %s.\n") == 0) {
        if (strcmp(lang_code, "ja") == 0) return "%sを追加しています。\n";
        return "Adding %s.\n";
    }
    if (strcmp(msg, "Unknown option %s.\n") == 0) {
        if (strcmp(lang_code, "ja") == 0) return "不明なオプション%s\n";
        return "Unknown option %s.\n";
    }
    if (strcmp(msg, "Specify a file.\n") == 0) {
        if (strcmp(lang_code, "ja") == 0) return "ファイルを指定してください。\n";
        return "Specify a file.\n";
    }
    if (strcmp(msg, "%s:%d: error: %s\n") == 0) {
        if (strcmp(lang_code, "ja") == 0) return "%s:%d: エラー: %s\n";
        return "%s:%d: error: %s\n";
    }
    if (strcmp(msg, "Out of memory.\n") == 0) {
        if (strcmp(lang_code, "ja") == 0) return "メモリが確保できません。\n";
        return "Out of memory.\n";
    }
    if (strcmp(msg, "shell(): Parameter not a string.\n") == 0) {
        if (strcmp(lang_code, "ja") == 0) return "shell(): 引数が文字列ではありません。\n";
        return "shell(): Parameter not a string.\n";
    }
    if (strcmp(msg, "Noct Programming Language ") == 0) {
        if (strcmp(lang_code, "ja") == 0) return "Noct プログラミング言語 ";
        return "Noct Programming Language ";
    }
    if (strcmp(msg, "Version %s\n") == 0) {
        if (strcmp(lang_code, "ja") == 0) return "バージョン %s\n";
        return "Version %s\n";
    }
    if (strcmp(msg, "JIT compilation is enabled.\n") == 0) {
        if (strcmp(lang_code, "ja") == 0) return "JITコンパイルが有効 ... 高速実行を開始します。\n";
        return "JIT compilation is enabled.\n";
    }
    if (strcmp(msg, "Entering REPL mode.\n") == 0) {
        if (strcmp(lang_code, "ja") == 0) return "REPLモードに入ります。\n";
        return "Entering REPL mode.\n";
    }
    if (strcmp(msg, "Usage\n") == 0) {
        if (strcmp(lang_code, "ja") == 0) return "使い方\n";
        return "Usage\n";
    }
    if (strcmp(msg, "  noct <file>                        ... run a program\n") == 0) {
        if (strcmp(lang_code, "ja") == 0) return "  noct <ファイル>                    ... プログラムを実行します\n";
        return "  noct <file>                        ... run a program\n";
    }
    if (strcmp(msg, "  noct --compile <files>             ... convert to bytecode files\n") == 0) {
        if (strcmp(lang_code, "ja") == 0) return "  noct --compile <ファイル...>       ... バイトコードに変換します\n";
        return "  noct --compile <files>             ... convert to bytecode files\n";
    }
    if (strcmp(msg, "  noct --ansic <出力> <入力...>      ... Cソースに変換します\n") == 0) {
        return "  noct --ansic <出力> <入力...>      ... Cソースに変換します\n";
    }
    if (strcmp(msg, "  noct --elisp <出力> <入力...>      ... Emacs Lispソースに変換します\n") == 0) {
        return "  noct --elisp <出力> <入力...>      ... Emacs Lispソースに変換します\n";
    }
    return msg;
}
