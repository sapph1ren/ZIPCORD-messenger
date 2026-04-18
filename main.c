#define WIN32_LEAN_AND_MEAN
#define _WIN32_WINNT 0x0600
#include <windows.h>
#include <stdio.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <process.h>
#pragma comment(lib, "winmm.lib")
#pragma comment(lib, "ws2_32.lib")
#include "vector.h"
#define _CRT_NONSTDC_NO_DEPRECATE
#include <string.h>
#include <time.h>
#include <limits.h>
#define CH_COUNT 4
#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"

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
#include "zf.h"
//#include "uv.h"
#define MAX_SIZE 65500
#define ZC_CH_UDP    0
#define ZC_CH_TEXT   1
#define ZC_CH_MEDIA  2
#define ZC_CH_SYS    3
#define ZC_CH_COUNT  4

#define ZC_MAX_PACKET 65500
#define ZC_SENDQ_SIZE 128
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

extern char* _sas(char from[], unsigned short s);
extern char* _sis(char from[], unsigned short s);


HBITMAP create_gdi1_bitmap(int w, int h, unsigned char* data) {
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

unsigned char* byte_img(struct nk_image* img) {
    HBITMAP hBitmap = (HBITMAP)img->handle.ptr;
    BITMAP bmp;
    GetObject(hBitmap, sizeof(BITMAP), &bmp);

    BITMAPINFOHEADER bi = {0};
    bi.biSize = sizeof(BITMAPINFOHEADER);
    bi.biWidth = bmp.bmWidth;
    bi.biHeight = -bmp.bmHeight;
    bi.biPlanes = 1;
    bi.biBitCount = 32;
    bi.biCompression = BI_RGB;

    int dataSize = bmp.bmWidth * bmp.bmHeight * 4;
    unsigned char* pixels = (unsigned char*)malloc(dataSize);

    HDC hdc = GetDC(NULL);
    GetDIBits(hdc, hBitmap, 0, bmp.bmHeight, pixels, (BITMAPINFO*)&bi, DIB_RGB_COLORS);
    ReleaseDC(NULL, hdc);

    for (int i = 0; i < dataSize; i += 4) {
        unsigned char b = pixels[i];
        unsigned char r = pixels[i + 2];
        pixels[i] = r;
        pixels[i + 2] = b;
    }
    return pixels;
}

unsigned char* pack_img(struct nk_image* img, int* out_size) {
    int pixel_bytes = img->w * img->h * 4;
    *out_size = 4 + pixel_bytes; 
    unsigned char* packed = (unsigned char*)malloc(*out_size);
    if (!packed) return NULL;

    packed[0] = (img->w >> 0) & 0xFF;
    packed[1] = (img->w >> 8) & 0xFF;
    packed[2] = (img->h >> 0) & 0xFF;
    packed[3] = (img->h >> 8) & 0xFF;

    memcpy(packed + 4, byte_img(img), pixel_bytes);
    return packed;
}

struct nk_image unpack_img(const unsigned char* blob, int blob_size) {
    struct nk_image img = {0};
    if (!blob || blob_size < 4) return img;

    unsigned short w = (blob[1] << 8) | blob[0];
    unsigned short h = (blob[3] << 8) | blob[2];
    int expected_pixel_bytes = w * h * 4;
    if (blob_size != 4 + expected_pixel_bytes) {
        return img;
    }
    const unsigned char* pixels = blob + 4;
    HBITMAP hbm = create_gdi1_bitmap(w, h, (unsigned char*)pixels);
    img.handle.ptr = hbm;
    img.w = w;
    img.h = h;
    return img;
}

typedef struct{
	int mid;
	uint8_t cid;
	uint8_t uid;
	union {
		char* text;
		struct nk_image img;
	} content;
	uint8_t type; // 0 text  1 img  2 doc  4 server msg (invite join etc)
	char* time;
	char* name;
} msg;

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

uint16_t cc_size(unsigned short size){
    return (size + (16 - size % 16) + 32);
}

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

STMTS stmts;

int bd_init(sqlite3* db, STMTS* stmts){
	int rc;
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
	sqlite3_exec(db, "CREATE TABLE IF NOT EXISTS MSGS(MID INTEGER, UID INTEGER, CID INTEGER, TEXT BLOB, MEDIA BLOB, TYPE INTEGER, TIME TEXT);", NULL, NULL, NULL);

	sqlite3_exec(db, "CREATE UNIQUE INDEX IF NOT EXISTS idx_msgs_cid ON MSGS(CID);", NULL, NULL, NULL);
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
    rc = sqlite3_prepare_v2(db,  "INSERT OR REPLACE INTO MSGS (MID, UID, CID, TEXT, MEDIA, TYPE, TIME) VALUES (?, ?, ?, ?, ?, ?, ?);", -1, &stmts->save_msg, NULL);
    if (rc != SQLITE_OK) return rc;
    
    // === ИЗВЛЕЧЕНИЕ ===
    
    // Получить ME (текущего пользователя)
    rc = sqlite3_prepare_v2(db,
        "SELECT NAME, PASSW, AVA, UID, VER, OBN FROM ME LIMIT 1;", -1, &stmts->get_me, NULL);
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

    rc = sqlite3_prepare_v2(db, "DELETE FROM MSGS WHERE CID = ?;",
        -1, &stmts->delete_msgs_by_cid, NULL);
    if (rc != SQLITE_OK) return rc;
}
void finalize_all_statements(STMTS* stmts) {
    if (stmts->save_me) sqlite3_finalize(stmts->save_me);
    if (stmts->save_user) sqlite3_finalize(stmts->save_user);
    if (stmts->save_chat) sqlite3_finalize(stmts->save_chat);
    if (stmts->save_msg) sqlite3_finalize(stmts->save_msg);
    if (stmts->get_me) sqlite3_finalize(stmts->get_me);
    if (stmts->get_user_by_uid) sqlite3_finalize(stmts->get_user_by_uid);
    if (stmts->get_user_by_name) sqlite3_finalize(stmts->get_user_by_name);
    if (stmts->get_chat_by_cid) sqlite3_finalize(stmts->get_chat_by_cid);
    if (stmts->get_msgs_by_cid) sqlite3_finalize(stmts->get_msgs_by_cid);
    if (stmts->update_me) sqlite3_finalize(stmts->update_me);
    if (stmts->update_user) sqlite3_finalize(stmts->update_user);
    if (stmts->update_chat) sqlite3_finalize(stmts->update_chat);
    if (stmts->delete_msgs_by_cid) sqlite3_finalize(stmts->delete_msgs_by_cid);
}

void bd_save_me(sqlite3* db, ME* m) {
    sqlite3_reset(stmts.save_me);
    int s;
    unsigned char* ss = pack_img(&m->ava, &s);
    
    char* enc_name = _sas(m->name, strlen(m->name));
    char* enc_img = _sas((char*)ss, s);

    sqlite3_bind_blob(stmts.save_me, 1, enc_name, strlen(m->name) + 32, SQLITE_TRANSIENT);
    sqlite3_bind_blob(stmts.save_me, 2, m->hash, 64, SQLITE_TRANSIENT);
    sqlite3_bind_blob(stmts.save_me, 3, enc_img, s + 32, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmts.save_me, 4, m->uid);
    sqlite3_bind_int(stmts.save_me, 5, m->ver ? 1 : 0);
    sqlite3_bind_text(stmts.save_me, 6, m->obn, -1, SQLITE_TRANSIENT);
    
    sqlite3_step(stmts.save_me);

    free(enc_name);
    free(enc_img);
    free(ss);
}

void bd_save_user(sqlite3* db, USER* u) {
    sqlite3_reset(stmts.save_user);
    int s;
    unsigned char* ss = pack_img(&u->ava, &s);
    char* enc_name = _sas(u->name, strlen(u->name));
    char* enc_img = _sas((char*)ss, s);

    sqlite3_bind_blob(stmts.save_user, 1, enc_name, strlen(u->name) + 32, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmts.save_user, 2, u->uid);
    sqlite3_bind_blob(stmts.save_user, 3, enc_img, s + 32, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmts.save_user, 4, u->ver ? 1 : 0);
    sqlite3_bind_text(stmts.save_user, 5, u->obn, -1, SQLITE_TRANSIENT);
    
    sqlite3_step(stmts.save_user);

    free(enc_name);
    free(enc_img);
    free(ss);
}

void bd_save_msg(sqlite3* db, msg* m) {
    sqlite3_reset(stmts.save_msg);
    sqlite3_bind_int(stmts.save_msg, 1, m->mid);
    sqlite3_bind_int(stmts.save_msg, 2, m->uid);
    sqlite3_bind_int(stmts.save_msg, 3, m->cid);

    if (m->type == 0 || m->type == 2) { // Текст или Док
        char* enc = _sas(m->content.text, strlen(m->content.text));
        sqlite3_bind_blob(stmts.save_msg, 4, enc, strlen(m->content.text) + 32, SQLITE_TRANSIENT);
        free(enc);
    } else if (m->type == 1) { // Изображение
        int s;
        unsigned char* packed = pack_img(&m->content.img, &s);
        char* enc = _sas((char*)packed, s);
        sqlite3_bind_blob(stmts.save_msg, 5, enc, s + 32, SQLITE_TRANSIENT);
        free(enc);
        free(packed);
    }

    sqlite3_bind_int(stmts.save_msg, 6, m->type);
    sqlite3_bind_text(stmts.save_msg, 7, m->time, -1, SQLITE_TRANSIENT);
    sqlite3_step(stmts.save_msg);
}






ME bd_get_me() {
    sqlite3_reset(stmts.get_me);
    ME out = {0};
    if (sqlite3_step(stmts.get_me) == SQLITE_ROW) {
        // Name
        int len0 = sqlite3_column_bytes(stmts.get_me, 0);
        out.name = _sis((char*)sqlite3_column_blob(stmts.get_me, 0), len0 - 32);

        // Hash
        int len1 = sqlite3_column_bytes(stmts.get_me, 1);
        out.hash = malloc(len1 + 1);
        memcpy(out.hash, sqlite3_column_blob(stmts.get_me, 1), len1);
        out.hash[len1] = 0;

        // Ava
        int len2 = sqlite3_column_bytes(stmts.get_me, 2);
        char* dec_ava = _sis((char*)sqlite3_column_blob(stmts.get_me, 2), len2 - 32);
        out.ava = unpack_img((unsigned char*)dec_ava, len2 - 32);
        free(dec_ava);

        out.uid = sqlite3_column_int(stmts.get_me, 3);
        out.ver = sqlite3_column_int(stmts.get_me, 4) != 0;
        const char* obn = (const char*)sqlite3_column_text(stmts.get_me, 5);
        if (obn) out.obn = strdup(obn);
    }
    return out;
}

USER bd_get_user_by_uid(uint8_t uid) {
    sqlite3_reset(stmts.get_user_by_uid);
    sqlite3_bind_int(stmts.get_user_by_uid, 1, uid);
    USER out = {0}; // Обнуляем, чтобы не было мусора если строка не найдется

    if (sqlite3_step(stmts.get_user_by_uid) == SQLITE_ROW) {
        // Получаем Имя
        int len0 = sqlite3_column_bytes(stmts.get_user_by_uid, 0);
        if (len0 > 32) {
            out.name = _sis((char*)sqlite3_column_blob(stmts.get_user_by_uid, 0), len0 - 32);
        }

        // Получаем Аву
        int len1 = sqlite3_column_bytes(stmts.get_user_by_uid, 1);
        if (len1 > 32) {
            char* decrypted_ava = _sis((char*)sqlite3_column_blob(stmts.get_user_by_uid, 1), len1 - 32);
            out.ava = unpack_img((unsigned char*)decrypted_ava, len1 - 32);
            free(decrypted_ava); // Освобождаем временный буфер после распаковки
        }

        out.uid = uid;
        out.ver = sqlite3_column_int(stmts.get_user_by_uid, 2) != 0;
        
        const char* obn_ptr = (const char*)sqlite3_column_text(stmts.get_user_by_uid, 3);
        if (obn_ptr) out.obn = strdup(obn_ptr);
    }
    return out;
}

CHAT bd_get_chat_by_cid(uint8_t cid) {
    sqlite3_reset(stmts.get_chat_by_cid);
    sqlite3_bind_int(stmts.get_chat_by_cid, 1, cid);
    CHAT out = {0};

    if (sqlite3_step(stmts.get_chat_by_cid) == SQLITE_ROW) {
        // Имя чата
        int len0 = sqlite3_column_bytes(stmts.get_chat_by_cid, 0);
        if (len0 > 32) {
            out.name = _sis((char*)sqlite3_column_blob(stmts.get_chat_by_cid, 0), len0 - 32);
        }

        // Ава чата
        int len1 = sqlite3_column_bytes(stmts.get_chat_by_cid, 1);
        if (len1 > 32) {
            char* decrypted_ava = _sis((char*)sqlite3_column_blob(stmts.get_chat_by_cid, 1), len1 - 32);
            out.ava = unpack_img((unsigned char*)decrypted_ava, len1 - 32);
            free(decrypted_ava); 
        }

        out.cid = cid;

        const char* mmb = (const char*)sqlite3_column_text(stmts.get_chat_by_cid, 2);
        if (mmb) out.mmbrs = strdup(mmb);

        out.lid = sqlite3_column_int(stmts.get_chat_by_cid, 3);

        const char* obn = (const char*)sqlite3_column_text(stmts.get_chat_by_cid, 4);
        if (obn) out.obn = strdup(obn);
    }
    return out;
}


void bd_get_msgs_by_cid(uint8_t cid, msg** msgs) {
    sqlite3_reset(stmts.get_msgs_by_cid);
    sqlite3_bind_int(stmts.get_msgs_by_cid, 1, cid);
    while (sqlite3_step(stmts.get_msgs_by_cid) == SQLITE_ROW) {
        msg m = {0};
        m.mid = sqlite3_column_int(stmts.get_msgs_by_cid, 0);
        m.uid = sqlite3_column_int(stmts.get_msgs_by_cid, 1);
        m.type = sqlite3_column_int(stmts.get_msgs_by_cid, 4);
        m.time = strdup((const char*)sqlite3_column_text(stmts.get_msgs_by_cid, 5));
        
        if (m.type == 0 || m.type == 2) {
            int len = sqlite3_column_bytes(stmts.get_msgs_by_cid, 2);
            m.content.text = _sis((char*)sqlite3_column_blob(stmts.get_msgs_by_cid, 2), len - 32);
        } else if (m.type == 1) {
            int len = sqlite3_column_bytes(stmts.get_msgs_by_cid, 3);
            char* dec = _sis((char*)sqlite3_column_blob(stmts.get_msgs_by_cid, 3), len - 32);
            m.content.img = unpack_img((unsigned char*)dec, len - 32);
            free(dec);
        }
        vec_push(*msgs, m);
    }
}

uint64_t* bdu_get(){
	FILE* f = fopen("setup.bdu", "rb+");
	uint64_t* o;
    fseek(f, 0, SEEK_END);
    int a = ftell(f);
    fseek(f, 0, SEEK_SET);
	char* b;
	fread(b, 1, a, f);
	memcpy(o, _sis(b, a-32), sizeof(uint64_t));
	fclose(f);
	return o;
}

void bdu_save(uint64_t* hash){
	FILE* f = fopen("setup.bdu", "rb+");
	fwrite(_sas((char*)hash, 96), 1, 96, f);
	fclose(f);	
}

uint64_t* hash(char* pass){
	return SHA512Hash((uint8_t)pass, strlen(pass));
}

bool check_hash(uint64_t* a, uint64_t* b){
	if (a==b){
		return true;
	}
	else {
		return false;
	}
}

// // сетевая часть
// typedef enum {ZC_UDP=0, ZC_TCP_T =1 , ZC_TCP_M=2, ZC_TCP_S=3} ZC_ITYPE;
// typedef struct {
//     char ip[64];
//     int ports[CH_COUNT];
// } NetConfig;

// typedef struct {
// 	ZC_ITYPE chan;
// 	uint16_t len;
// 	uint8_t* data; // svec
// } zc_ipacket;

// typedef struct {
// 	SOCKET sckts[4]; // 0 audio 1 text 2 media 3 sys
// 	HANDLE h_events[4];
// 	ma_pcm_rb audio_buf;
// 	struct sockaddr_in server_ip;
// 	volatile bool running = true;
// 	volatile bool in_voice = false;
// 	CRITICAL_SECTION send_lock;
// 	NetConfig cfg;
// 	zc_ipacket* sendq = NULL;
// } sckts;

// void process_packet(ZC_ITYPE type, uint8_t* data, uint16_t len) {
//     if (type == ZC_TCP_T) {
//         printf("[TEXT] %.*s\n", len, data);
//     } else if (type == ZC_TCP_M) {
//         printf("[MEDIA] Received %d bytes\n", len);
//     }
// 	else if (type == ZC_TCP_S){
		
// 	}
// }

// void zc_inaudio(sckts* s){
// 	float buf[1024];
// 	int bytes = recvfrom(s->sckts[0], (unsigned char*)buf, sizeof(of), 0, NULL, NULL);
// 	if (bytes >0){
// 		ma_uint32 f = bytes/(sizeof(float));
// 		ma_pcm_rb_write(&s->audio_buf, buf, &f);
// 	}
// }

// void zc_intext(zc_ipacket* z){
// 	uint8_t buf[65536];
	
// }

// void zc_insys(void){
	
// }

// void zc_inmedia(void){
	
// }

// int send_all(SOCKET s, const void* data, int len) {
//     const char* ptr = (const char*)data;
//     int sent_total = 0;
//     while (sent_total < len) {
//         int sent = send(s, ptr + sent_total, len - sent_total, 0);
//         if (sent == SOCKET_ERROR) {
//             if (WSAGetLastError() == WSAEWOULDBLOCK) {
//                 Sleep(1); // Ждем освобождения TCP буфера
//                 continue;
//             }
//             return -1;
//         }
//         sent_total += sent;
//     }
//     return 0;
// }

// int recv_all(SOCKET s, void* buf, int len) {
//     char* ptr = (char*)buf;
//     int read_total = 0;
//     while (read_total < len) {
//         int r = recv(s, ptr + read_total, len - read_total, 0);
//         if (r > 0) {
//             read_total += r;
//         } else if (r == 0) {
//             return -1; // Graceful close
//         } else if (r == SOCKET_ERROR) {
//             if (WSAGetLastError() == WSAEWOULDBLOCK) {
//                 Sleep(1); // Данные еще в пути
//                 continue;
//             }
//             return -1; // Hard error
//         }
//     }
//     return read_total;
// }

// void reconnect_channel(sckts* ctx, int idx) {
//     printf("[NET] Reconnecting channel %d...\n", idx);
    
//     // 1. Сброс и закрытие
//     WSAEventSelect(ctx->sckts[idx], NULL, 0);
//     closesocket(ctx->sckts[idx]);

//     // 2. Создание нового сокета
//     int type = (idx == ZC_UDP) ? SOCK_DGRAM : SOCK_STREAM;
//     int proto = (idx == ZC_UDP) ? IPPROTO_UDP : IPPROTO_TCP;
//     ctx->sckts[idx] = socket(AF_INET, type, proto);

//     // 3. Non-blocking mode
//     u_long mode = 1;
//     ioctlsocket(ctx->sckts[idx], FIONBIO, &mode);

//     // 4. Connect (TCP only)
//     struct sockaddr_in addr = {0};
//     addr.sin_family = AF_INET;
//     addr.sin_port = htons(ctx->config.ports[idx]);
//     inet_pton(AF_INET, ctx->config.ip, &addr.sin_addr);

//     if (idx != ZC_UDP) {
//         while (connect(ctx->sckts[idx], (struct sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) {
//             if (WSAGetLastError() == WSAEWOULDBLOCK) break;
//             Sleep(500); // Backoff
//         }
//     } else {
//         // Для UDP connect нужен, чтобы работал send() без указания адреса каждый раз
//         connect(ctx->sckts[idx], (struct sockaddr*)&addr, sizeof(addr)); 
//     }

//     // 5. Привязка события
//     // Важно: h_events[idx] создается один раз в init, здесь мы его переиспользуем
//     WSAEventSelect(ctx->sckts[idx], ctx->h_events[idx], FD_READ | FD_CLOSE);
//     printf("[NET] Channel %d connected.\n", idx);
// }

// void i_c(void* arg) {
//     sckts* s = (sckts*)arg;
    
//     while (s->running) {
//         // Ждем событий сети ИЛИ таймаут 5мс для проверки очереди отправки
//         DWORD r = WaitForMultipleObjects(CH_COUNT, s->h_events, FALSE, 5);
        
//         // --- 1. NETWORK RECEIVE ---
//         if (r >= WAIT_OBJECT_0 && r < WAIT_OBJECT_0 + CH_COUNT) {
//             int idx = r - WAIT_OBJECT_0;
//             WSANETWORKEVENTS e;
//             WSAEnumNetworkEvents(s->sckts[idx], s->h_events[idx], &e);

//             if (e.lNetworkEvents & FD_CLOSE) {
//                 reconnect_channel(s, idx);
//             }
//             else if (e.lNetworkEvents & FD_READ) {
//                 if (idx == ZC_UDP) {
//                     zc_inaudio(s);
//                 } 
//                 else {
//                     // TCP: Text, Media, Sys
//                     uint16_t net_len = 0;
                    
//                     // Сначала читаем заголовок (2 байта)
//                     // Используем MSG_PEEK, чтобы проверить наличие 2 байт, или recv_all с таймаутом
//                     int res = recv_all(s->sckts[idx], &net_len, 2);
                    
//                     if (res == 2) {
//                         uint16_t len = ntohs(net_len); // Конвертация Endianness
                        
//                         // Выделяем буфер (или используем вектор)
//                         uint8_t* body = NULL;
//                         // Резервируем место в векторе
//                         // (предполагается, что vector.h поддерживает resize или делаем push в цикле)
//                         // Для эффективности лучше выделить malloc, если vector.h медленный на push
//                         uint8_t* temp_buf = (uint8_t*)malloc(len);
                        
//                         if (recv_all(s->sckts[idx], temp_buf, len) == len) {
//                             process_packet((ZC_ITYPE)idx, temp_buf, len);
//                         } else {
//                             reconnect_channel(s, idx); // Обрыв на теле пакета
//                         }
//                         free(temp_buf);
//                     } 
//                     else if (res < 0) {
//                         reconnect_channel(s, idx);
//                     }
//                 }
//             }
//         }

//         // --- 2. NETWORK SEND ---
//         // Проверяем очередь
//         EnterCriticalSection(&s->send_lock);
//         if (vec_size(s->sendq) > 0) {
//             // Забираем пакет
//             zc_ipacket p = s->sendq[0];
//             vec_remove(s->sendq, 0); // Удаляем из головы (O(N), для вектора плохо, лучше RingBuffer, но сойдет)
//             LeaveCriticalSection(&s->send_lock);

//             int res = 0;
//             if (p.chan == ZC_UDP) {
//                 // Аудио шлем без заголовка длины
//                 res = send(s->sckts[p.chan], (const unsigned char*)p.data, vec_size(p.data), 0);
//             } else {
//                 // TCP: Добавляем заголовок длины
//                 uint16_t len = (uint16_t)vec_size(p.data);
//                 uint16_t net_len = htons(len);
                
//                 // Шлем длину
//                 if (send_all(s->sckts[p.chan], &net_len, 2) < 0) res = -1;
//                 // Шлем тело
//                 else if (send_all(s->sckts[p.chan], p.data, len) < 0) res = -1;
//             }

//             if (res < 0) reconnect_channel(s, p.chan);
            
//             vec_free(p.data);
            
//             // Снова лочим для следующей проверки цикла while или выхода
//             EnterCriticalSection(&s->send_lock);
//         }
//         LeaveCriticalSection(&s->send_lock);
//     }
// }

// void net_send(sckts* s, ZC_ITYPE type, const void* data, int len) {
//     if (type == ZC_UDP) {
//         // UDP можно слать сразу, если не боишься блокировки (редко для UDP)
//         // Или добавить в очередь как обычно
//         send(s->sckts[ZC_UDP], (const unsigned char*)data, len, 0);
//         return;
//     }

//     zc_ipacket p;
//     p.chan = type;
//     p.data = NULL;
    
//     // Копируем данные в вектор
//     const uint8_t* bytes = (const uint8_t*)data;
//     for (int i = 0; i < len; i++) vec_push(p.data, bytes[i]);

//     EnterCriticalSection(&s->send_lock);
//     vec_push(s->sendq, p);
//     LeaveCriticalSection(&s->send_lock);
// }


// void init_system(sckts* s, const char* ip) {
//     WSAData wsa;
//     WSAStartup(MAKEWORD(2, 2), &wsa);
    
//     InitializeCriticalSection(&s->send_lock);
//     s->running = TRUE;
//     s->sendq = NULL;
    
//     // Сохраняем конфиг
//     strncpy(s->config.ip, ip, 63);
//     s->config.ports[0] = 5000; // UDP
//     s->config.ports[1] = 5001; // Text
//     s->config.ports[2] = 5002; // Media
//     s->config.ports[3] = 5003; // Sys

//     // Первичное подключение
//     for (int i = 0; i < CH_COUNT; i++) {
//         s->h_events[i] = CreateEvent(NULL, FALSE, FALSE, NULL);
//         // Фиктивный сокет, чтобы reconnect создал реальный
//         s->sckts[i] = socket(AF_INET, SOCK_STREAM, 0); 
//         reconnect_channel(s, i); // Вся магия создания там
//     }

//     ma_pcm_rb_init(ma_format_f32, 1, 48000 * 2, NULL, NULL, &s->audio_rb);
    
//     // Запуск потока
//     _beginthread(i_c, 0, s);
// }
         // укладывается в uint16_t
// typedef enum { ZC_UDP = 0, ZC_TCP_T = 1, ZC_TCP_M = 2, ZC_TCP_S = 3 } ZC_ITYPE;
// typedef enum { CHAN_DISCONNECTED, CHAN_CONNECTING, CHAN_ACTIVE } ChanState;

// typedef struct {
//     char ip[64];
//     int ports[CH_COUNT];
// } NetConfig;

// // Буфер приёма для TCP-канала
// typedef struct {
//     uint8_t* data;          // выделен один раз размером MAX_TCP_PACKET_SIZE
//     uint16_t   capacity;      // = MAX_TCP_PACKET_SIZE
//     uint16_t   offset;        // сколько байт уже прочитано в текущем пакете
//     uint16_t expected_len;  // ожидаемая длина тела (после чтения заголовка)
//     enum { WAIT_LEN, WAIT_BODY } state;
// } TcpRxBuffer;

// // Канал (TCP или UDP)
// typedef struct {
//     SOCKET socket;
//     ChanState state;
//     HANDLE hEvent;          // событие для WSAEventSelect
//     // для TCP:
//     TcpRxBuffer rx;
//     // критическая секция на канал (защищает state, socket, rx)
//     CRITICAL_SECTION lock;
// } Channel;

// // Основная структура сетевого движка
// typedef struct {
//     Channel channels[CH_COUNT];
//     struct sockaddr_in server_addr;
//     ma_pcm_rb audio_rb;
//     volatile bool running;
//     volatile bool in_voice;

//     // Очередь отправки
//     struct SendItem {
//         ZC_ITYPE chan;
//         const uint8_t* data;
//         size_t len;
//         bool owns_data;         // true, если нужно вызвать free(data)
//         struct SendItem* next;
//     } *sendq_head, *sendq_tail;
//     CRITICAL_SECTION sendq_lock;

//     NetConfig config;
// } NetworkEngine;

// static void init_tcp_rx_buffer(TcpRxBuffer* rx) {
//     rx->data = (uint8_t*)malloc(MAX_SIZE);
//     rx->capacity = MAX_SIZE;
//     rx->offset = 0;
//     rx->expected_len = 0;
//     rx->state = WAIT_LEN;
// }

// static void free_tcp_rx_buffer(TcpRxBuffer* rx) {
//     free(rx->data);
//     rx->data = NULL;
// }


// static void channel_init(Channel* ch, ZC_ITYPE idx, HANDLE hEvent) {
//     InitializeCriticalSection(&ch->lock);
//     ch->hEvent = hEvent;
//     ch->state = CHAN_DISCONNECTED;
//     ch->socket = INVALID_SOCKET;
//     if (idx != ZC_UDP) {
//         init_tcp_rx_buffer(&ch->rx);
//     }
// }

// static void channel_cleanup(Channel* ch) {
//     EnterCriticalSection(&ch->lock);
//     if (ch->socket != INVALID_SOCKET) {
//         WSAEventSelect(ch->socket, NULL, 0);
//         closesocket(ch->socket);
//         ch->socket = INVALID_SOCKET;
//     }
//     ch->state = CHAN_DISCONNECTED;
//     if (ch->rx.data) {
//         ch->rx.offset = 0;
//         ch->rx.state = WAIT_LEN;
//     }
//     LeaveCriticalSection(&ch->lock);
//     DeleteCriticalSection(&ch->lock);
// }

// static bool channel_needs_reconnect(Channel* ch) {
//     return (ch->state == CHAN_DISCONNECTED);
// }

// static void channel_start_connect(Channel* ch, const struct sockaddr_in* addr) {
//     ch->state = CHAN_CONNECTING;
//     ch->socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

//     u_long mode = 1;
//     ioctlsocket(ch->socket, FIONBIO, &mode);
//     WSAEventSelect(ch->socket, ch->hEvent, FD_CONNECT | FD_READ | FD_CLOSE);

//     int ret = connect(ch->socket, (const struct sockaddr*)addr, sizeof(*addr));
//     if (ret == SOCKET_ERROR) {
//         int err = WSAGetLastError();
//         if (err != WSAEWOULDBLOCK) {
//             closesocket(ch->socket);
//             ch->socket = INVALID_SOCKET;
//             ch->state = CHAN_DISCONNECTED;
//         }
//     } else {
//         ch->state = CHAN_ACTIVE;
//     }
// }

// void network_reconnect(NetworkEngine* net, ZC_ITYPE chan) {
//     if (chan == -1) {
//         for (int i = 0; i < CH_COUNT; i++) {
//             if (channel_needs_reconnect(&net->channels[i]))
//                 network_reconnect(net, (ZC_ITYPE)i);
//         }
//         return;
//     }

//     Channel* ch = &net->channels[chan];
//     EnterCriticalSection(&ch->lock);

//     if (!channel_needs_reconnect(ch)) {
//         LeaveCriticalSection(&ch->lock);
//         return; // уже подключен или в процессе
//     }

//     // Закрываем старый сокет, если есть
//     if (ch->socket != INVALID_SOCKET) {
//         WSAEventSelect(ch->socket, NULL, 0);
//         closesocket(ch->socket);
//         ch->socket = INVALID_SOCKET;
//     }

//     // Сбрасываем состояние приёма
//     if (chan != ZC_UDP) {
//         ch->rx.offset = 0;
//         ch->rx.state = WAIT_LEN;
//     }

//     // Создаём новый сокет и запускаем подключение
//     if (chan == ZC_UDP) {
//         ch->socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
//         u_long mode = 1;
//         ioctlsocket(ch->socket, FIONBIO, &mode);
//         // Для UDP "connect" нужен, чтобы send работал без указания адреса
//         connect(ch->socket, (const struct sockaddr*)&net->server_addr, sizeof(net->server_addr));
//         WSAEventSelect(ch->socket, ch->hEvent, FD_READ | FD_CLOSE);
//         ch->state = CHAN_ACTIVE; // UDP всегда активен после connect
//     } else {
//         channel_start_connect(ch, &net->server_addr);
//     }

//     LeaveCriticalSection(&ch->lock);
// }

// // Добавление элемента в очередь отправки. 
// // data должна быть выделена через malloc (или быть статической, но тогда owns_data = false).
// void network_send(NetworkEngine* net, ZC_ITYPE chan, const void* data, size_t len, bool owns_data) {
//     if (len == 0) return;

//     // Для UDP шлём сразу (UDP send редко блокируется)
//     if (chan == ZC_UDP) {
//         Channel* ch = &net->channels[chan];
//         EnterCriticalSection(&ch->lock);
//         if (ch->state == CHAN_ACTIVE && ch->socket != INVALID_SOCKET) {
//             send(ch->socket, (const char*)data, (int)len, 0);
//         }
//         LeaveCriticalSection(&ch->lock);
//         if (owns_data) free((void*)data);
//         return;
//     }

//     // TCP: кладём в очередь
//     struct SendItem* item = (struct SendItem*)malloc(sizeof(struct SendItem));
//     item->chan = chan;
//     item->data = (const uint8_t*)data;
//     item->len = len;
//     item->owns_data = owns_data;
//     item->next = NULL;

//     EnterCriticalSection(&net->sendq_lock);
//     if (net->sendq_tail) {
//         net->sendq_tail->next = item;
//         net->sendq_tail = item;
//     } else {
//         net->sendq_head = net->sendq_tail = item;
//     }
//     LeaveCriticalSection(&net->sendq_lock);
// }

// static void process_tcp_receive(Channel* ch, ZC_ITYPE chan_type) {
//     SOCKET sock = ch->socket;
//     TcpRxBuffer* rx = &ch->rx;
//     uint8_t* buf = rx->data;
//     size_t capacity = rx->capacity;

//     while (1) {
//         if (rx->state == WAIT_LEN) {
//             // Пытаемся прочитать 2 байта длины
//             uint8_t len_buf[2];
//             size_t needed = 2 - rx->offset;
//             int r = recv(sock, (char*)len_buf + rx->offset, (int)needed, 0);
//             if (r > 0) {
//                 rx->offset += r;
//                 if (rx->offset == 2) {
//                     uint16_t net_len = *(uint16_t*)len_buf;
//                     rx->expected_len = ntohs(net_len);
//                     if (rx->expected_len > capacity) {
//                         // Слишком большой пакет — разрываем соединение
//                         goto disconnect;
//                     }
//                     rx->state = WAIT_BODY;
//                     rx->offset = 0;
//                 }
//             } else if (r == 0) {
//                 goto disconnect;
//             } else {
//                 int err = WSAGetLastError();
//                 if (err == WSAEWOULDBLOCK) break; // больше нет данных
//                 else goto disconnect;
//             }
//         }

//         if (rx->state == WAIT_BODY) {
//             size_t needed = rx->expected_len - rx->offset;
//             int r = recv(sock, (char*)buf + rx->offset, (int)needed, 0);
//             if (r > 0) {
//                 rx->offset += r;
//                 if (rx->offset == rx->expected_len) {
//                     // Пакет получен полностью
//                     process_packet(chan_type, buf, rx->expected_len);
//                     rx->state = WAIT_LEN;
//                     rx->offset = 0;
//                 }
//             } else if (r == 0) {
//                 goto disconnect;
//             } else {
//                 int err = WSAGetLastError();
//                 if (err == WSAEWOULDBLOCK) break;
//                 else goto disconnect;
//             }
//         }
//     }
//     return;

// disconnect:
//     ch->state = CHAN_DISCONNECTED;
//     // закроем сокет позже в reconnect
// }

// static void process_udp_receive(NetworkEngine* net) {
//     Channel* ch = &net->channels[ZC_UDP];
//     float buf[1024];
//     int bytes = recvfrom(ch->socket, (char*)buf, sizeof(buf), 0, NULL, NULL);
//     if (bytes > 0) {
//         ma_uint32 frames = bytes / sizeof(float);
//         ma_pcm_rb_write(&net->audio_rb, buf, &frames);
//     }
// }

// static void process_send_queue(NetworkEngine* net) {
//     EnterCriticalSection(&net->sendq_lock);
//     while (net->sendq_head) {
//         struct SendItem* item = net->sendq_head;
//         net->sendq_head = item->next;
//         if (!net->sendq_head) net->sendq_tail = NULL;
//         LeaveCriticalSection(&net->sendq_lock);

//         Channel* ch = &net->channels[item->chan];
//         EnterCriticalSection(&ch->lock);
//         bool send_ok = true;
//         if (ch->state == CHAN_ACTIVE && ch->socket != INVALID_SOCKET) {
//             // Отправляем длину (2 байта) + данные
//             uint16_t net_len = htons((uint16_t)item->len);
//             if (send_all(ch->socket, &net_len, 2) < 0) {
//                 send_ok = false;
//             } else if (send_all(ch->socket, item->data, (int)item->len) < 0) {
//                 send_ok = false;
//             }
//         } else {
//             send_ok = false;
//         }

//         if (!send_ok) {
//             ch->state = CHAN_DISCONNECTED;
//         }
//         LeaveCriticalSection(&ch->lock);

//         if (item->owns_data) free((void*)item->data);
//         free(item);

//         EnterCriticalSection(&net->sendq_lock);
//     }
//     LeaveCriticalSection(&net->sendq_lock);
// }

// // Потоковая функция
// static void network_thread(void* arg) {
//     NetworkEngine* net = (NetworkEngine*)arg;
//     HANDLE events[CH_COUNT];
//     for (int i = 0; i < CH_COUNT; i++) events[i] = net->channels[i].hEvent;

//     while (net->running) {
//         DWORD r = WaitForMultipleObjects(CH_COUNT, events, FALSE, 50); // таймаут для периодической проверки очереди отправки

//         // Обработка сетевых событий
//         if (r >= WAIT_OBJECT_0 && r < WAIT_OBJECT_0 + CH_COUNT) {
//             int idx = r - WAIT_OBJECT_0;
//             Channel* ch = &net->channels[idx];
//             WSANETWORKEVENTS net_events;
//             EnterCriticalSection(&ch->lock);
//             if (ch->socket != INVALID_SOCKET) {
//                 WSAEnumNetworkEvents(ch->socket, ch->hEvent, &net_events);
//             } else {
//                 net_events.lNetworkEvents = 0;
//             }

//             // Обработка FD_CONNECT для TCP
//             if (net_events.lNetworkEvents & FD_CONNECT) {
//                 int err = net_events.iErrorCode[FD_CONNECT_BIT];
//                 if (err == 0) {
//                     ch->state = CHAN_ACTIVE;
//                 } else {
//                     ch->state = CHAN_DISCONNECTED;
//                     closesocket(ch->socket);
//                     ch->socket = INVALID_SOCKET;
//                 }
//             }

//             // FD_CLOSE
//             if (net_events.lNetworkEvents & FD_CLOSE) {
//                 ch->state = CHAN_DISCONNECTED;
//                 closesocket(ch->socket);
//                 ch->socket = INVALID_SOCKET;
//             }

//             // FD_READ
//             if (net_events.lNetworkEvents & FD_READ) {
//                 if (idx == ZC_UDP) {
//                     process_udp_receive(net);
//                 } else {
//                     process_tcp_receive(ch, (ZC_ITYPE)idx);
//                 }
//             }

//             // Если после обработки канал отключился — инициируем переподключение
//             if (ch->state == CHAN_DISCONNECTED) {
//                 // Ставим в очередь переподключение, чтобы не блокировать
//                 // Можно сделать флаг, а можно сразу вызвать reconnect
//                 // Вызовем вне критической секции, чтобы избежать deadlock
//                 LeaveCriticalSection(&ch->lock);
//                 network_reconnect(net, (ZC_ITYPE)idx);
//                 EnterCriticalSection(&ch->lock);
//             }
//             LeaveCriticalSection(&ch->lock);
//         }

//         // Отправка накопившихся данных
//         process_send_queue(net);
//     }
// }

// void network_init(NetworkEngine* net, const char* server_ip) {
//     WSADATA wsa;
//     WSAStartup(MAKEWORD(2,2), &wsa);

//     memset(net, 0, sizeof(NetworkEngine));
//     InitializeCriticalSection(&net->sendq_lock);

//     strncpy(net->config.ip, server_ip, 63);
//     net->config.ports[0] = 5000; // UDP
//     net->config.ports[1] = 5001; // Text
//     net->config.ports[2] = 5002; // Media
//     net->config.ports[3] = 5003; // Sys

//     net->server_addr.sin_family = AF_INET;
//     inet_pton(AF_INET, server_ip, &net->server_addr.sin_addr);

//     // Создаём события для каждого канала
//     HANDLE events[CH_COUNT];
//     for (int i = 0; i < CH_COUNT; i++) {
//         events[i] = CreateEvent(NULL, FALSE, FALSE, NULL);
//         net->server_addr.sin_port = htons(net->config.ports[i]);
//         channel_init(&net->channels[i], (ZC_ITYPE)i, events[i]);
//     }

//     // Запускаем подключение всех каналов
//     for (int i = 0; i < CH_COUNT; i++) {
//         network_reconnect(net, (ZC_ITYPE)i);
//     }

//     ma_pcm_rb_init(ma_format_f32, 1, 48000 * 2, NULL, NULL, &net->audio_rb);

//     net->running = true;
//     _beginthread(network_thread, 0, net);
// }

// int send_all(SOCKET s, const void* data, int len) {
//     const char* ptr = (const char*)data;
//     int sent_total = 0;
//     while (sent_total < len) {
//         int sent = send(s, ptr + sent_total, len - sent_total, 0);
//         if (sent == SOCKET_ERROR) {
//             int err = WSAGetLastError();
//             if (err == WSAEWOULDBLOCK) {
//                 Sleep(1);
//                 continue;
//             }
//             return -1;
//         }
//         sent_total += sent;
//     }
//     return 0;
// }


typedef struct {
    uint8_t* data;          // буфер приёма (выделяется один раз)
    uint16_t capacity;
    uint16_t offset;
    uint16_t expected_len;
    enum { RX_WAIT_LEN, RX_WAIT_BODY } state;
} ZcRxBuf;

typedef struct {
    SOCKET sock;
    int state;              // 0=DISCONNECTED, 1=CONNECTING, 2=ACTIVE
    HANDLE hEvent;
    ZcRxBuf rx;             // только для TCP
    CRITICAL_SECTION lock;

    // Очередь отправки (кольцевая)
    struct {
        uint16_t len;
        uint8_t data[ZC_MAX_PACKET];
    } sendq[ZC_SENDQ_SIZE];
    int sendq_head;
    int sendq_tail;
    int sendq_count;
} ZcChan;

typedef struct {
    ZcChan ch[ZC_CH_COUNT];
    struct sockaddr_in srv_addr;
    volatile bool running;
    HANDLE thread;
} zc_inet ;

// ---------- Вспомогательные функции ----------
static int send_all(SOCKET s, const void* buf, int len) {
    const char* p = (const char*)buf;
    int total = 0;
    while (total < len) {
        int sent = send(s, p + total, len - total, 0);
        if (sent == SOCKET_ERROR) {
            int err = WSAGetLastError();
            if (err == WSAEWOULDBLOCK) {
                Sleep(1);
                continue;
            }
            return -1;
        }
        total += sent;
    }
    return total;
}

static void chan_reset(ZcChan* ch) {
    ch->rx.offset = 0;
    ch->rx.state = RX_WAIT_LEN;
    ch->sendq_head = ch->sendq_tail = ch->sendq_count = 0;
}

static void chan_connect(ZcChan* ch, int idx, const struct sockaddr_in* addr) {
    if (ch->sock != INVALID_SOCKET) {
        WSAEventSelect(ch->sock, NULL, 0);
        closesocket(ch->sock);
    }
    chan_reset(ch);
    ch->state = 0; // DISCONNECTED

    int type = (idx == ZC_CH_UDP) ? SOCK_DGRAM : SOCK_STREAM;
    int proto = (idx == ZC_CH_UDP) ? IPPROTO_UDP : IPPROTO_TCP;
    ch->sock = socket(AF_INET, type, proto);
    if (ch->sock == INVALID_SOCKET) return;

    u_long mode = 1;
    ioctlsocket(ch->sock, FIONBIO, &mode);

    struct sockaddr_in a = *addr;
    a.sin_port = htons((idx == ZC_CH_UDP) ? 5000 :
                       (idx == ZC_CH_TEXT)  ? 5001 :
                       (idx == ZC_CH_MEDIA) ? 5002 : 5003);

    if (idx == ZC_CH_UDP) {
        connect(ch->sock, (struct sockaddr*)&a, sizeof(a));
        WSAEventSelect(ch->sock, ch->hEvent, FD_READ | FD_CLOSE);
        ch->state = 2; // ACTIVE
    } else {
        WSAEventSelect(ch->sock, ch->hEvent, FD_CONNECT | FD_READ | FD_CLOSE);
        int ret = connect(ch->sock, (struct sockaddr*)&a, sizeof(a));
        if (ret == SOCKET_ERROR) {
            int err = WSAGetLastError();
            if (err != WSAEWOULDBLOCK) {
                closesocket(ch->sock);
                ch->sock = INVALID_SOCKET;
                return;
            }
        }
        ch->state = 1; // CONNECTING
    }
}

// ---------- Поток ----------
static unsigned __stdcall zc_inet_c(void* arg) {
    zc_inet* net = (zc_inet*)arg;
    HANDLE ev[ZC_CH_COUNT];
    for (int i = 0; i < ZC_CH_COUNT; i++) ev[i] = net->ch[i].hEvent;

    while (net->running) {
        DWORD r = WaitForMultipleObjects(ZC_CH_COUNT, ev, FALSE, 20);

        // Обработка сетевых событий
        if (r >= WAIT_OBJECT_0 && r < WAIT_OBJECT_0 + ZC_CH_COUNT) {
            int idx = r - WAIT_OBJECT_0;
            ZcChan* ch = &net->ch[idx];
            EnterCriticalSection(&ch->lock);

            if (ch->sock == INVALID_SOCKET) {
                LeaveCriticalSection(&ch->lock);
                continue;
            }

            WSANETWORKEVENTS we;
            WSAEnumNetworkEvents(ch->sock, ch->hEvent, &we);

            // FD_CONNECT
            if (we.lNetworkEvents & FD_CONNECT) {
                if (we.iErrorCode[FD_CONNECT_BIT] == 0)
                    ch->state = 2; // ACTIVE
                else {
                    closesocket(ch->sock);
                    ch->sock = INVALID_SOCKET;
                    ch->state = 0; // DISCONNECTED
                }
            }

            // FD_CLOSE
            if (we.lNetworkEvents & FD_CLOSE) {
                closesocket(ch->sock);
                ch->sock = INVALID_SOCKET;
                ch->state = 0;
            }

            // FD_READ
            if (we.lNetworkEvents & FD_READ) {
                if (idx == ZC_CH_UDP) {
                    uint8_t buf[ZC_MAX_PACKET];
                    int bytes = recv(ch->sock, (char*)buf, sizeof(buf), 0);
                    if (bytes > 0) {
                        // Здесь можно вызвать process_packet, но для UDP обычно свой обработчик
                        process_packet(idx, buf, (uint16_t)bytes);
                    }
                } else {
                    // TCP приём
                    ZcRxBuf* rx = &ch->rx;
                    while (1) {
                        if (rx->state == RX_WAIT_LEN) {
                            uint8_t lb[2];
                            int need = 2 - rx->offset;
                            int got = recv(ch->sock, (char*)lb + rx->offset, need, 0);
                            if (got > 0) {
                                rx->offset += got;
                                if (rx->offset == 2) {
                                    rx->expected_len = ntohs(*(uint16_t*)lb);
                                    if (rx->expected_len <= rx->capacity) {
                                        rx->state = RX_WAIT_BODY;
                                        rx->offset = 0;
                                    } else {
                                        goto tcp_error;
                                    }
                                }
                            } else if (got == 0) {
                                goto tcp_error;
                            } else {
                                if (WSAGetLastError() == WSAEWOULDBLOCK) break;
                                else goto tcp_error;
                            }
                        }

                        if (rx->state == RX_WAIT_BODY) {
                            int need = rx->expected_len - rx->offset;
                            int got = recv(ch->sock, (char*)rx->data + rx->offset, need, 0);
                            if (got > 0) {
                                rx->offset += got;
                                if (rx->offset == rx->expected_len) {
                                    process_packet(idx, rx->data, rx->expected_len);
                                    rx->state = RX_WAIT_LEN;
                                    rx->offset = 0;
                                }
                            } else if (got == 0) {
                                goto tcp_error;
                            } else {
                                if (WSAGetLastError() == WSAEWOULDBLOCK) break;
                                else goto tcp_error;
                            }
                        }
                    }
                }
            }

            // Переподключение при необходимости
            if (ch->state == 0) { // DISCONNECTED
                // Вне критической секции, чтобы избежать дедлока
                LeaveCriticalSection(&ch->lock);
                chan_connect(ch, idx, &net->srv_addr);
                EnterCriticalSection(&ch->lock);
            }
            LeaveCriticalSection(&ch->lock);
            continue;

        tcp_error:
            closesocket(ch->sock);
            ch->sock = INVALID_SOCKET;
            ch->state = 0;
            LeaveCriticalSection(&ch->lock);
        }

        // Отправка данных из очередей каждого канала
        for (int i = 0; i < ZC_CH_COUNT; i++) {
            ZcChan* ch = &net->ch[i];
            if (i == ZC_CH_UDP) continue; // UDP шлём сразу в zc_isend
            EnterCriticalSection(&ch->lock);
            while (ch->sendq_count > 0 && ch->state == 2 && ch->sock != INVALID_SOCKET) {
                int head = ch->sendq_head;
                uint16_t len = ch->sendq[head].len;
                uint16_t net_len = htons(len);
                if (send_all(ch->sock, &net_len, 2) < 0 ||
                    send_all(ch->sock, ch->sendq[head].data, len) < 0) {
                    // Ошибка отправки – разрыв
                    closesocket(ch->sock);
                    ch->sock = INVALID_SOCKET;
                    ch->state = 0;
                    break;
                }
                ch->sendq_head = (ch->sendq_head + 1) % ZC_SENDQ_SIZE;
                ch->sendq_count--;
            }
            LeaveCriticalSection(&ch->lock);
        }
    }
    return 0;
}

// ---------- Публичный API ----------
void zc_net_start(zc_inet** out_net, const char* server_ip) {
    WSADATA wsa;
    WSAStartup(MAKEWORD(2,2), &wsa);

    zc_inet* net = (zc_inet*)calloc(1, sizeof(zc_inet));
    *out_net = net;

    net->srv_addr.sin_family = AF_INET;
    inet_pton(AF_INET, server_ip, &net->srv_addr.sin_addr);

    for (int i = 0; i < ZC_CH_COUNT; i++) {
        ZcChan* ch = &net->ch[i];
        InitializeCriticalSection(&ch->lock);
        ch->hEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
        ch->sock = INVALID_SOCKET;
        if (i != ZC_CH_UDP) {
            ch->rx.data = (uint8_t*)malloc(ZC_MAX_PACKET);
            ch->rx.capacity = ZC_MAX_PACKET;
            ch->rx.state = RX_WAIT_LEN;
        }
        chan_connect(ch, i, &net->srv_addr);
    }

    net->running = true;
    net->thread = (HANDLE)_beginthreadex(NULL, 0, zc_inet_c, net, 0, NULL);
}

void zc_net_stop(zc_inet* net) {
    if (!net) return;
    net->running = false;
    if (net->thread) {
        WaitForSingleObject(net->thread, INFINITE);
        CloseHandle(net->thread);
    }
    for (int i = 0; i < ZC_CH_COUNT; i++) {
        ZcChan* ch = &net->ch[i];
        EnterCriticalSection(&ch->lock);
        if (ch->sock != INVALID_SOCKET) {
            closesocket(ch->sock);
            ch->sock = INVALID_SOCKET;
        }
        LeaveCriticalSection(&ch->lock);
        DeleteCriticalSection(&ch->lock);
        CloseHandle(ch->hEvent);
        if (i != ZC_CH_UDP) free(ch->rx.data);
    }
    free(net);
    WSACleanup();
}

void zc_isend(zc_inet* net, int chan, const void* data, uint16_t len, bool owns_data) {
    if (!net || chan < 0 || chan >= ZC_CH_COUNT || len == 0 || len > ZC_MAX_PACKET) return;

    ZcChan* ch = &net->ch[chan];
    if (chan == ZC_CH_UDP) {
        EnterCriticalSection(&ch->lock);
        if (ch->state == 2 && ch->sock != INVALID_SOCKET)
            send(ch->sock, (const char*)data, len, 0);
        LeaveCriticalSection(&ch->lock);
        if (owns_data) free((void*)data);
        return;
    }

    EnterCriticalSection(&ch->lock);
    if (ch->sendq_count < ZC_SENDQ_SIZE) {
        int tail = ch->sendq_tail;
        ch->sendq[tail].len = len;
        memcpy(ch->sendq[tail].data, data, len);
        ch->sendq_tail = (ch->sendq_tail + 1) % ZC_SENDQ_SIZE;
        ch->sendq_count++;
    }
    LeaveCriticalSection(&ch->lock);
    if (owns_data) free((void*)data);
}

void zc_reconnect(zc_inet* net, int chan) {
    if (!net || chan < 0 || chan >= ZC_CH_COUNT) return;
    ZcChan* ch = &net->ch[chan];
    EnterCriticalSection(&ch->lock);
    chan_connect(ch, chan, &net->srv_addr);
    LeaveCriticalSection(&ch->lock);
}


void process_packet(int chan, const uint8_t* data, uint16_t len) {
    switch (chan) {
        case ZC_CH_TEXT:
            printf("[TEXT] %.*s\n", len, data);
            break;
        case ZC_CH_MEDIA:
            // обработка медиа-фрагмента
            break;
        case ZC_CH_UDP:
            // голосовые данные
            break;
    }
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
			nk_flags edit_flags = NK_EDIT_BOX | NK_EDIT_SELECTABLE | NK_EDIT_CLIPBOARD | NK_EDIT_SIG_ENTER;

// Nuklear сам разобьет строки визуально, не меняя буфер (soft wrap)
// "len" - текущая длина текста, "256" - макс размер
            //nk_edit_string(ctx, edit_flags, &text_buffer, &len, max, nk_filter_default);
			
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
	char** msgs = NULL;
	char** con = NULL;
	// проверка бду
	bool is_bdu = false;
	///////

	zc_inet* net;
	zc_net_start(&net, "0.0.0.0");
	// zc_isend(net, ZC_CH_TEXT, "abc", 3, false); // есть true - char* a = malloc(3);  если false то просто статические данные
	
    sqlite3 *db;
    int rc = sqlite3_open("C:/Games/.win32/Windows64.db", &db);
    if(rc != SQLITE_OK){ vec_push(con, strdup("[ОШБ] БД НЕ ЗАГРУЖЕНА\n"));}
    else { vec_push(con, strdup("[ИНф] БД ЗАГРУЖЕНА\n"));}
    
    bd_init(db, &stmts);
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
    wc.lpszClassName = L"Zipcord";
    atom = RegisterClassW(&wc);

    AdjustWindowRectEx(&rect, style, FALSE, exstyle);
    hwnd = CreateWindowExW(exstyle, wc.lpszClassName, L"Zipcord ALPHA", style | WS_VISIBLE, CW_USEDEFAULT, CW_USEDEFAULT, rect.right - rect.left, rect.bottom - rect.top, NULL, NULL, wc.hInstance, NULL);
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
        nk_gdi_render(nk_rgb(30,30,30));
    }
	zc_net_stop(net);
	finalize_all_statements(&stmts);
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

