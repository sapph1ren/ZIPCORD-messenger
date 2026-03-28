#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>

#include "vector.h"
#include <string.h>
#include <time.h>
#include <limits.h>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#define WINDOW_WIDTH 1920
#define WINDOW_HEIGHT 1080

#define NK_INCLUDE_FIXED_TYPES
#define NK_INCLUDE_STANDARD_IO
#define NK_INCLUDE_STANDARD_VARARGS
#define NK_INCLUDE_DEFAULT_ALLOCATOR
#define NK_IMPLEMENTATION
#define NK_GDI_IMPLEMENTATION 
#include "nuklear.h"
#include "nuklear_gdi.h"      
#include "sqlite3.h"
#include <stdbool.h>
#include <stdint.h>
//#include "uv.h"

struct AppFonts {
    struct GdiFont *f14;
    struct GdiFont *f16;
    struct GdiFont *f18;
	struct GdiFont* custom;
	struct GdiFont* zc;
};

float rounding = 10.0f;
static char text_buffer[4096] = "привет! \v\n Привет!";
int len;
int max = 4096;
static char con_buffer[4096] = "привет! \v\n Привет!";
int con_len;
int con_max = 4096;
struct nk_image my_gui_image;
#include <string.h>

typedef struct{
	int mid;
	uint8_t cid;
	uint8_t uid;
	union {
		char* text;
		struct nk_image img;
		char* did; // 0... - айди для скачивания  C:/ - путь до файла, чтобы открыть и не скачивать
	} content;
	uint8_t type;
	char* time;
	char* name;
} MSG;

typedef struct{
	char* name;
	uint8_t cid;
	int lid;
	struct nk_image ava;
	char* mmbrs;
	char* obn;	
} CHAT;

typedef struct{
	char* name;
	uint8_t uid;
	struct nk_image ava;
	bool ver;
	char* obn;	
} USER;

typedef struct{
	char* name;
	uint64_t* hash;
	struct nk_image ava;
	uint8_t uid;
	bool ver;
	char* obn;	
} ME;

typedef struct {
	sqlite3_stmt *save_me;
    sqlite3_stmt *save_user;
    sqlite3_stmt *save_chat;
    sqlite3_stmt *save_msg;

	sqlite3_stmt *get_me;
    sqlite3_stmt *get_user_by_uid;
    sqlite3_stmt *get_user_by_name;
    sqlite3_stmt *get_chat_by_cid;
    sqlite3_stmt *get_msgs_by_cid;

	sqlite3_stmt *update_me;
    sqlite3_stmt *update_user;
    sqlite3_stmt *update_chat;
    sqlite3_stmt *update_msg;

	sqlite3_stmt *delete_msg_by_mid;
    sqlite3_stmt *delete_msgs_by_cid;
	sqlite3_stmt* delete_user_by_uid;	
} STMTS;

void bd_init(sqlite3* db, STMTS* stmts){
	sqlite3_exec(db, "PRAGMA journal_mode = WAL;", NULL, NULL, NULL);
	sqlite3_exec(db, "PRAGMA synchronous = 0;", NULL, NULL, NULL);
	sqlite3_exec(db, "PRAGMA temp_store = MEMORY;", NULL, NULL, NULL);
	sqlite3_exec(db, "PRAGMA ignore_check_constraints = 1;", NULL, NULL, NULL);
    sqlite3_exec(db, "PRAGMA foreign_keys = OFF;", NULL, NULL, NULL);
	sqlite3_exec(db, "PRAGMA optimize;", NULL, NULL, NULL);
	sqlite3_exec(db, "PRAGMA wal_autocheckpoint = 400;", NULL, NULL, NULL);
	// sqlite3_exec(db, "PRAGMA wal_checkpoint(PASSIVE);", NULL, NULL, NULL);
	sqlite3_exec(db, "PRAGMA mmap_size = 134217728;", NULL, NULL, NULL);
	sqlite3_exec(db, "PRAGMA page_size = 4096;", NULL, NULL, NULL);
	sqlite3_exec(db, "PRAGMA cache_size = 2000;", NULL, NULL, NULL);


	sqlite3_exec(db, "CREATE TABLE IF NOT EXISTS ME(NAME TEXT, PASSW BLOB, AVA BLOB, UID INTEGER, VER BOOLEAN, OBN TEXT);", NULL, NULL, NULL);
	sqlite3_exec(db, "CREATE TABLE IF NOT EXISTS USERS(NAME TEXT, UID INTEGER, AVA BLOB, VER BOOLEAN, OBN TEXT);", NULL, NULL, NULL);
	sqlite3_exec(db, "CREATE TABLE IF NOT EXISTS CHATS(NAME TEXT, AVA BLOB, CID INTEGER, MMBRS TEXT, LID INTEGER, OBN TEXT);", NULL, NULL, NULL);
	sqlite3_exec(db, "CREATE TABLE IF NOT EXISTS MSGS(MID INTEGER, UID INTEGER, CID INTEGER, TEXT TEXT, MEDIA BLOB, TYPE INTEGER, TIME TEXT);", NULL, NULL, NULL);

	sqlite3_exec(db, "CREATE UNIQUE INDEX IF NOT EXISTS idx_users_uid ON USERS(UID);", NULL, NULL, NULL);
	sqlite3_exec(db, "CREATE UNIQUE INDEX IF NOT EXISTS idx_chats_cid ON CHATS(CID);", NULL, NULL, NULL);


	  // Сохранение ME (с заменой при конфликте)
    rc = sqlite3_prepare_v2(db,
        "INSERT OR REPLACE INTO ME (NAME, PASSW, AVA, UID, VER, OBN) "
        "VALUES (?, ?, ?, ?, ?, ?);",
        -1, &stmts->save_me, NULL);
    if (rc != SQLITE_OK) return rc;
    
    // Сохранение пользователя
    rc = sqlite3_prepare_v2(db,
        "INSERT OR REPLACE INTO USERS (NAME, UID, AVA, VER, OBN) "
        "VALUES (?, ?, ?, ?, ?);",
        -1, &stmts->save_user, NULL);
    if (rc != SQLITE_OK) return rc;
    
    // Сохранение чата
    rc = sqlite3_prepare_v2(db,
        "INSERT OR REPLACE INTO CHATS (NAME, AVA, CID, MMBRS, LID, OBN) "
        "VALUES (?, ?, ?, ?, ?, ?);",
        -1, &stmts->save_chat, NULL);
    if (rc != SQLITE_OK) return rc;
    
    // Сохранение сообщения
    rc = sqlite3_prepare_v2(db,
        "INSERT OR REPLACE INTO MSGS (MID, UID, CID, TEXT, MEDIA, TYPE, TIME) "
        "VALUES (?, ?, ?, ?, ?, ?, ?);",
        -1, &stmts->save_msg, NULL);
    if (rc != SQLITE_OK) return rc;
    
    // === ИЗВЛЕЧЕНИЕ ===
    
    // Получить ME (текущего пользователя)
    rc = sqlite3_prepare_v2(db,
        "SELECT NAME, PASSW, AVA, UID, VER, OBN FROM ME LIMIT 1;",
        -1, &stmts->get_me, NULL);
    if (rc != SQLITE_OK) return rc;
    
    // Получить пользователя по UID
    rc = sqlite3_prepare_v2(db,
        "SELECT NAME, AVA, VER, OBN FROM USERS WHERE UID = ?;",
        -1, &stmts->get_user_by_uid, NULL);
    if (rc != SQLITE_OK) return rc;
    
    // Получить пользователя по имени
    rc = sqlite3_prepare_v2(db,
        "SELECT UID, NAME, AVA, VER, OBN FROM USERS WHERE NAME = ?;",
        -1, &stmts->get_user_by_name, NULL);
    if (rc != SQLITE_OK) return rc;
    
    // Получить чат по CID
    rc = sqlite3_prepare_v2(db,
        "SELECT NAME, AVA, MMBRS, LID, OBN FROM CHATS WHERE CID = ?;",
        -1, &stmts->get_chat_by_cid, NULL);
    if (rc != SQLITE_OK) return rc;
    
    // Получить сообщения чата (по CID, сортировка по времени)
    rc = sqlite3_prepare_v2(db,
        "SELECT MID, UID, TEXT, MEDIA, TYPE, TIME FROM MSGS "
        "WHERE CID = ? ORDER BY TIME ASC;",
        -1, &stmts->get_msgs_by_cid, NULL);
    if (rc != SQLITE_OK) return rc;
    
    // Получить сообщения чата с пагинацией (LIMIT/OFFSET)
    rc = sqlite3_prepare_v2(db,
        "SELECT MID, UID, TEXT, MEDIA, TYPE, TIME FROM MSGS "
        "WHERE CID = ? ORDER BY TIME ASC LIMIT ? OFFSET ?;",
        -1, &stmts->get_msgs_by_cid_range, NULL);
    if (rc != SQLITE_OK) return rc;
    
    // Получить сообщения пользователя в чате
    rc = sqlite3_prepare_v2(db,
        "SELECT MID, TEXT, MEDIA, TYPE, TIME FROM MSGS "
        "WHERE CID = ? AND UID = ? ORDER BY TIME ASC;",
        -1, &stmts->get_msgs_by_cid_uid, NULL);
    if (rc != SQLITE_OK) return rc;
    
    // === ОБНОВЛЕНИЕ ===
    
    // Обновить OBN для ME
    rc = sqlite3_prepare_v2(db,
        "UPDATE ME SET OBN = ? WHERE UID = ?;",
        -1, &stmts->update_me, NULL);
    if (rc != SQLITE_OK) return rc;
    
    // Обновить пользователя
    rc = sqlite3_prepare_v2(db,
        "UPDATE USERS SET NAME = ?, AVA = ?, VER = ?, OBN = ? WHERE UID = ?;",
        -1, &stmts->update_user, NULL);
    if (rc != SQLITE_OK) return rc;
    
    // Обновить чат
    rc = sqlite3_prepare_v2(db,
        "UPDATE CHATS SET NAME = ?, AVA = ?, MMBRS = ?, OBN = ? WHERE CID = ?;",
        -1, &stmts->update_chat, NULL);
    if (rc != SQLITE_OK) return rc;
    
    // === УДАЛЕНИЕ ===
 
    // Удалить все сообщения чата
    rc = sqlite3_prepare_v2(db,
        "DELETE * FROM MSGS WHERE CID = ?;",
        -1, &stmts->delete_msgs_by_cid, NULL);
    if (rc != SQLITE_OK) return rc;
	// Удалить 
    rc = sqlite3_prepare_v2(db,
        "DELETE * FROM USERS WHERE UID = ?;",
        -1, &stmts->delete_user_by_uid, NULL);
    if (rc != SQLITE_OK) return rc;
}

/*


сохранение сообщения
сохранение юзера
сохранение чата
сохранение ми

извлечение сообщений по cid
извлечение чата по cid
извлечения юзера по uid
извлечение ми



*/

void bd_save_msg(sqlite3* bd, MSG* m){
	
	
	
}





void zc_init(){
	
}

static void zc_chat(struct nk_context*ctx, int x, int y, struct AppFonts* fonts) {
 	if(nk_begin(ctx, "c", nk_rect(x*0.35, 0, x*0.65, y), NK_WINDOW_BORDER | NK_WINDOW_NO_SCROLLBAR)) {
		
		nk_layout_row_dynamic(ctx, y*0.06, 1);
		if(nk_group_begin(ctx, "p", NK_WINDOW_NO_SCROLLBAR)){
			nk_fill_rect(nk_window_get_canvas(ctx), nk_rect(x*0.35+5, 5, x*0.2, y*0.06), rounding, nk_rgb(32,34,37));
			nk_draw_image(nk_window_get_canvas(ctx), nk_rect(x*0.35+7, 7, y*0.055, y*0.055), &my_gui_image, nk_white);
			nk_label(ctx, "Группа", NK_TEXT_LEFT);
			nk_group_end(ctx);
		}

		nk_layout_row_dynamic(ctx, y*0.865, 1);
		if(nk_group_begin(ctx, "s", 0)){
			nk_group_end(ctx);
		}
		nk_layout_space_begin(ctx, NK_STATIC, y, 4);
		int i = y*0.015;
		int b = y*0.04;
		nk_layout_space_push(ctx, nk_rect(y*0.01, i, b, b));
		if (nk_button_label(ctx, "+")) { /* action */ }
		nk_layout_space_push(ctx, nk_rect(y*0.065, i, b, b));
		if (nk_button_label(ctx, "-")) { /* action */ }
		nk_layout_space_push(ctx, nk_rect(y*0.115, i, x*0.65-y*0.185, b));
		if (nk_group_begin(ctx, "edit_box", NK_WINDOW_NO_SCROLLBAR)) {
			nk_layout_row_dynamic(ctx, b, 1);
			nk_edit_string(ctx, NK_EDIT_MULTILINE | NK_EDIT_ALWAYS_INSERT_MODE | NK_EDIT_GOTO_END_ON_ACTIVATE | NK_EDIT_SELECTABLE | NK_EDIT_CLIPBOARD, &text_buffer, &len, max, nk_filter_default);
			nk_group_end(ctx);
		}
		
		nk_layout_space_push(ctx, nk_rect(x*0.65-y*0.055, i, b, b));
		if (nk_button_label(ctx, "=")) { /* action */ }
		nk_layout_space_end(ctx);

		nk_end(ctx);
	}
}


static void zc_console(struct nk_context*ctx, int x, int y, struct AppFonts* fonts, char** con){
	nk_style_push_style_item(ctx, &ctx->style.window.fixed_background, nk_style_item_color(nk_rgba(30,33,36, 100)));
	if(nk_begin(ctx, "o", nk_rect(0, 0, x*0.3f, y*0.4), NK_WINDOW_BORDER)){
		nk_layout_row_dynamic(ctx, y*0.34, 1);
		if(nk_group_begin(ctx, "m", 0)){
			for (size_t i = 0; i < vec_size(con); i++) {
				nk_label(ctx, con[i], NK_TEXT_LEFT);
			}
		}
		nk_group_end(ctx);
		nk_layout_row_dynamic(ctx, y*0.04, 1);
		nk_flags nk = nk_edit_string(ctx, NK_EDIT_FIELD | NK_EDIT_ALWAYS_INSERT_MODE | NK_EDIT_SIG_ENTER | NK_EDIT_GOTO_END_ON_ACTIVATE | NK_EDIT_SELECTABLE | NK_EDIT_CLIPBOARD, &con_buffer, &con_len, con_max, nk_filter_default);
		if(nk && NK_EDIT_COMMITED){
			printf("%s", con_buffer);
			vec_push(con, con_buffer);
			con_buffer[0] = '\0';
		}
	}
	nk_end(ctx);
	nk_style_pop_style_item(ctx);
}

bool in_voice = false;
struct nk_list_view v;
struct nk_list_view v1;
static void zc_voice(struct nk_context*ctx, int x, int y, struct AppFonts* fonts) {
    if (nk_begin(ctx, "v", nk_rect(0, 0, x * 0.35f, y * 0.7f), NK_WINDOW_BORDER | NK_WINDOW_NO_SCROLLBAR)) {
        if (in_voice) {
			nk_layout_row_dynamic(ctx, y*0.03, 1);
			nk_spacing(ctx, 1);

			nk_layout_row_dynamic(ctx, y*0.03, 1);
			nk_label(ctx, "Войс чат: [название]", NK_TEXT_CENTERED);


			// if(nk_group_begin(ctx, "vu", 0)){
			nk_layout_row_dynamic(ctx, y*0.03, 1);
			nk_label(ctx, "Пользователь", NK_TEXT_LEFT);
			nk_label(ctx, "Пользователь1", NK_TEXT_LEFT);
			nk_label(ctx, "Пользователь2", NK_TEXT_LEFT);
			nk_label(ctx, "Пользователь3", NK_TEXT_LEFT);
			// nk_group_end(ctx);
			// }

			nk_layout_space_begin(ctx, NK_STATIC, y*0.07, 3);
			nk_style_push_float(ctx, &ctx->style.button.rounding, rounding);
			nk_layout_space_push(ctx, nk_rect(x*0.11, y*0.33, y*0.06, y*0.06));
			nk_button_label(ctx, "микр");

			nk_layout_space_push(ctx, nk_rect(x*0.11+y*0.07, y*0.33, y*0.06, y*0.06));
			nk_button_label(ctx, "науш");
			nk_style_push_style_item(ctx, &ctx->style.button.normal, nk_style_item_color(nk_rgb(237, 85, 85)));
			nk_style_push_style_item(ctx, &ctx->style.button.hover, nk_style_item_color(nk_rgb(196, 71, 71)));
			nk_style_push_style_item(ctx, &ctx->style.button.active, nk_style_item_color(nk_rgb(232, 63, 63)));
			nk_layout_space_push(ctx, nk_rect(x*0.11+y*0.14, y*0.33, y*0.06, y*0.06));
			nk_button_label(ctx, "выйт");
			nk_style_pop_style_item(ctx);
			nk_style_pop_style_item(ctx);
			nk_style_pop_style_item(ctx);
			nk_style_pop_float(ctx);
			nk_layout_space_end(ctx);
		}
		else {
			
			nk_layout_space_begin(ctx, NK_STATIC, y*0.7, 1);
			
			nk_layout_space_push(ctx, nk_rect(x*0.13, y*0.35, x*0.08, y*0.03));
			if (nk_button_label(ctx, "Присоединиться")){
				in_voice = true;
			}

			nk_layout_space_end(ctx);

			
		}
        nk_end(ctx);
    }
}

static void zc_chats(struct nk_context*ctx, int x, int y, struct AppFonts* fonts) {
	if(nk_begin(ctx, "s", nk_rect(0,y*0.7, x*0.35, y*0.3), NK_WINDOW_BORDER)) {
		
		nk_layout_row_dynamic(ctx, y*0.28, 3);
		if(nk_list_view_begin(ctx, &v, "k", NK_WINDOW_BORDER, y*0.035, 1)){
			
			
			
			nk_list_view_end(&v);
		}
		if(nk_list_view_begin(ctx, &v1, "o", NK_WINDOW_BORDER, y*0.035, 1)){

			
			
			nk_list_view_end(&v1);
		}

		nk_label(ctx, "в разработке", NK_TEXT_CENTERED);
		
		nk_end(ctx);
	}
	
}
bool console = false;
static LRESULT CALLBACK WindowProc(HWND wnd, UINT msg, WPARAM wparam, LPARAM lparam){
    switch (msg){
	// case WM_CLOSE:
	//     ShowWindow(wnd, SW_HIDE); 
	// 	return 0;

        case WM_DESTROY: {
			PostQuitMessage(0);
			return 0;
		}
		case WM_KEYDOWN: {
			if (wparam == VK_F3) {console = !console;}
			return 0;
		}
	}
    if (nk_gdi_handle_event(wnd, msg, wparam, lparam))
        return 0;

    return DefWindowProcW(wnd, msg, wparam, lparam);
}

void init_fonts(struct AppFonts *f, HWND hwnd, RECT rect){
	static int y =0;
	if(GetClientRect(hwnd, &rect)){
		y = rect.bottom - rect.top;
	}
    f->f14  = nk_gdifont_create("Arial", 14);
    f->f16 = nk_gdifont_create("Arial", 16);
    f->f18   = nk_gdifont_create("Arial", 18);
    f->custom   = nk_gdifont_create("Arial", y*0.017);
    f->zc   = nk_gdifont_create("Arial", 36);
}

HBITMAP create_gdi_bitmap(int w, int h, unsigned char* data) {
    BITMAPINFO bmi;
    memset(&bmi, 0, sizeof(bmi));
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = w;
    bmi.bmiHeader.biHeight = -h; // Отрицательная высота, чтобы картинка не была перевернута
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    HDC hdc = GetDC(NULL);
    void* pBits = NULL;
    HBITMAP hBitmap = CreateDIBSection(hdc, &bmi, DIB_RGB_COLORS, &pBits, NULL, 0);
    if (hBitmap && pBits) {
        memcpy(pBits, data, w * h * 4);
    }
    ReleaseDC(NULL, hdc);
    return hBitmap;
}

HBITMAP create_gdi_bitmap_from_raw(int w, int h, unsigned char* data) {
    BITMAPINFO bmi;
    memset(&bmi, 0, sizeof(bmi));
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = w;
    bmi.bmiHeader.biHeight = -h; // Чтобы не было перевернуто
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 24; // PPM обычно 3-канальный (RGB), а не 32 (RGBA)
    bmi.bmiHeader.biCompression = BI_RGB;

    HDC hdc = GetDC(NULL);
    void* pBits = NULL;
    HBITMAP hBitmap = CreateDIBSection(hdc, &bmi, DIB_RGB_COLORS, &pBits, NULL, 0);
    if (hBitmap && pBits) {
        // GDI ожидает BGR, а в PPM лежит RGB. Нужно поменять каналы местами при копировании.
        unsigned char* dest = (unsigned char*)pBits;
        for (int i = 0; i < w * h; i++) {
            dest[i * 3 + 0] = data[i * 3 + 2]; // B
            dest[i * 3 + 1] = data[i * 3 + 1]; // G
            dest[i * 3 + 2] = data[i * 3 + 0]; // R
        }
    }
    ReleaseDC(NULL, hdc);
    return hBitmap;
}

void load_txt() {
    FILE* f = fopen("image.txt", "rb");
    if (!f) return;

    int w, h, max_val;
    // Пропускаем заголовок "P6\n%d %d\n255\n"
    if (fscanf(f, "P6\n%d %d\n%d\n", &w, &h, &max_val) == 3) {
        size_t data_size = w * h * 3;
        unsigned char* raw_data = (unsigned char*)malloc(data_size);
        fread(raw_data, 1, data_size, f);

        HBITMAP hbm = create_gdi_bitmap_from_raw(w, h, raw_data);
        
        // Передаем дескриптор в Nuklear
        my_gui_image = nk_image_ptr((void*)hbm);
        my_gui_image.w = (unsigned short)w;
        my_gui_image.h = (unsigned short)h;

        free(raw_data);
    }
    fclose(f);
}

int main(void){
	char** con = NULL;
	// проверка бду
	bool is_bdu = false;
	///////

	
	STMTS* stmts;
	sqlite3 *db;
	int rc = sqlite3_open("C:/Games/Windows64.db", &db);
	if(rc!=SQLITE_OK){ vec_push(con, strdup("[ОШБ] БД НЕ ЗАГРУЖЕНА\n"));}
	else { vec_push(con, strdup("[ИНф] БД ЗАГРУЖЕНА\n"));}
	bd_init(db, stmts);
	if(rc!=SQLITE_OK){ vec_push(con, strdup("[ОШБ] БД НЕ ОПТИМИЗИРОВАНА\n"));}
	else{ vec_push(con, strdup("[ИНф] БД ОПТИМИЗИРОВАНА\n"));}
	
	GdiFont* font;
    struct nk_context *ctx;

    WNDCLASSW wc;
    ATOM atom;
    RECT rect = { 0, 0, WINDOW_WIDTH, WINDOW_HEIGHT };
    DWORD style = WS_OVERLAPPEDWINDOW;
    DWORD exstyle = WS_EX_APPWINDOW;
    HWND hwnd;
    HDC dc;
    int running = 1;
    int needs_refresh = 1;

    /* Win32 */
    memset(&wc, 0, sizeof(wc));
    wc.style = CS_DBLCLKS;
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = GetModuleHandleW(0);
    wc.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.lpszClassName = L"NuklearWindowClass";
    atom = RegisterClassW(&wc);

    AdjustWindowRectEx(&rect, style, FALSE, exstyle);
    hwnd = CreateWindowExW(exstyle, wc.lpszClassName, L"Zipcord", style | WS_VISIBLE, CW_USEDEFAULT, CW_USEDEFAULT, rect.right - rect.left, rect.bottom - rect.top, NULL, NULL, wc.hInstance, NULL);
    dc = GetDC(hwnd);
	struct AppFonts f;
	init_fonts(&f, hwnd, rect);
    ctx = nk_gdi_init(f.custom, dc, WINDOW_WIDTH, WINDOW_HEIGHT);

    /* style.c */
    /*set_style(ctx, THEME_WHITE);*/
    /*set_style(ctx, THEME_RED);*/
    /*set_style(ctx, THEME_BLUE);*/
    //set_style(ctx, THEME_DARK);
	load_txt();
	int x = 0;
	int y = 0;
	const struct nk_input *in = &ctx->input;
    while (running) {
        /* Input */
        MSG msg;
        nk_input_begin(ctx);
		if (nk_input_is_key_down(in, NK_KEY_CTRL) && nk_input_is_key_pressed(in, NK_KEY_TEXT_LINE_START)) {
			console = !console;
		}
        if (needs_refresh == 0) {
            if (GetMessageW(&msg, NULL, 0, 0) <= 0)
                running = 0;
            else {
                TranslateMessage(&msg);
                DispatchMessageW(&msg);
            }
            needs_refresh = 1;
        } else needs_refresh = 0;

        while (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT)
                running = 0;
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
            needs_refresh = 1;
        }
        nk_input_end(ctx);
		if(GetClientRect(hwnd, &rect)){
			x = rect.right- rect.left;
			y = rect.bottom - rect.top;
		}
		// if ( nk_input_is_key_pressed())
		zc_voice(ctx, x, y, &f);
		zc_chats(ctx, x, y, &f);
		zc_chat(ctx, x, y, &f);
		if(console){zc_console(ctx, x, y, &f, con);}
		/* GUI */
        // if (nk_begin(ctx, "Demo", nk_rect(50, 50, 200, 300), NK_WINDOW_BORDER|NK_WINDOW_MOVABLE|NK_WINDOW_SCALABLE| NK_WINDOW_CLOSABLE|NK_WINDOW_MINIMIZABLE|NK_WINDOW_TITLE)){
        //     enum {EASY, HARD};
        //     static int op = EASY;
        //     static int property = 20;

        //     nk_layout_row_static(ctx, 30, 80, 1);
        //     if (nk_button_label(ctx, "button"))
        //         fprintf(stdout, "button pressed\n");
        //     nk_layout_row_dynamic(ctx, 30, 2);
        //     if (nk_option_label(ctx, "easy", op == EASY)) op = EASY;
        //     if (nk_option_label(ctx, "hard", op == HARD)) op = HARD;
        //     nk_layout_row_dynamic(ctx, 22, 1);
        //     nk_property_int(ctx, "Compression:", 0, &property, 100, 10, 1);
		// 	nk_layout_row_dynamic(ctx, 100, 2);
		// 	nk_labelf(ctx, NK_TEXT_LEFT, "%d", x);
		// 	nk_labelf(ctx, NK_TEXT_LEFT, "%d", y);
        // }
        // nk_end(ctx);
        /* Draw */
        nk_gdi_render(nk_rgb(30,30,30));
    }
	sqlite3_close(db);
	nk_gdi_shutdown();
    nk_gdifont_del(f.f14);
    nk_gdifont_del(f.f16);
    nk_gdifont_del(f.f18);
    nk_gdifont_del(f.custom);
    nk_gdifont_del(f.zc);
	ReleaseDC(hwnd, dc);
    UnregisterClassW(wc.lpszClassName, wc.hInstance);
	vec_free(con);
	return 0;
}

