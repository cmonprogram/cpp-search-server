# Search-server
Программа для поиска документов по набору слов
***

## Развертывание
```
g++ -o search_server -std=c++17 -O3 -I ./search-server *.cpp search-server/*.h
./search_server
```
# Формат входных данных
```
Создает объект поискового сервера (с параметром: слова которые не учитываются при обработке запроса)
SearchServer search_server("and with"s);

Добавляет документ (с параметрами: id, содержимое, статус документа, пользовательские оценки)
search_server.AddDocument(++id, text, DocumentStatus::ACTUAL, {1, 2});

Находит документы, соответствующее набору слов из запроса (с параметрами: способ выполнения, слова запроса, фильтр)
search_server.FindTopDocuments(execution::seq, "curly nasty cat"s, DocumentStatus::BANNED)
```
# Формат выходных данных
```
Выводит строку формата:
{ document_id = 2, relevance = 0.866434, rating = 1 }
document_id   - id документа
relevance     - коэффициент соответствия запросу
rating        - пользовательский рейтинг документа
```  
## Использование
### Ввод
```
void PrintDocument(const Document& document) {
    cout << "{ "s
         << "document_id = "s << document.id << ", "s
         << "relevance = "s << document.relevance << ", "s
         << "rating = "s << document.rating << " }"s << endl;
}
int main() {
    SearchServer search_server("and with"s);
    int id = 0;
    for (
        const string& text : {
            "white cat and yellow hat"s,
            "curly cat curly tail"s,
            "nasty dog with big eyes"s,
            "nasty pigeon john"s,
        }
    ) {
        search_server.AddDocument(++id, text, DocumentStatus::ACTUAL, {1, 2});
    }
    cout << "ACTUAL by default:"s << endl;
    // последовательная версия
    for (const Document& document : search_server.FindTopDocuments("curly nasty cat"s)) {
        PrintDocument(document);
    }
    cout << "BANNED:"s << endl;
    // последовательная версия
    for (const Document& document : search_server.FindTopDocuments(execution::seq, "curly nasty cat"s, DocumentStatus::BANNED)) {
        PrintDocument(document);
    }
    cout << "Even ids:"s << endl;
    // параллельная версия
    for (const Document& document : search_server.FindTopDocuments(execution::par, "curly nasty cat"s, [](int document_id, DocumentStatus status, int rating) { return document_id % 2 == 0; })) {
        PrintDocument(document);
    }
    return 0;
}
```
### Вывод
```
ACTUAL by default:
{ document_id = 2, relevance = 0.866434, rating = 1 }
{ document_id = 4, relevance = 0.231049, rating = 1 }
{ document_id = 1, relevance = 0.173287, rating = 1 }
{ document_id = 3, relevance = 0.173287, rating = 1 }
BANNED:
Even ids:
{ document_id = 2, relevance = 0.866434, rating = 1 }
{ document_id = 4, relevance = 0.231049, rating = 1 }
```
