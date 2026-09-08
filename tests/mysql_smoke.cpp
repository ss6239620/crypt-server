#include <iostream>
#include <mysql/mysql.h>

using namespace std;

int main() {
    // Initialize MySQL connection
    MYSQL *conn;
    conn = mysql_init(NULL);

    // MySQL Server credentials (Use the correct database name)
    const char *server = "";
    const char *user = "";          // Your MySQL username
    const char *password = "";       // Your MySQL password
    const char *database = "";      // Your existing database

    // Connect to MySQL Server
    if (!mysql_real_connect(conn, server, user, password, database, 0, NULL, 0)) {
        cerr << "❌ MySQL Connection Failed: " << mysql_error(conn) << endl;
        return 1;
    }

    cout << "✅ Connected to MySQL successfully!" << endl;

    // Create `user` table (Ensure database is selected)
    const char *create_table = "CREATE TABLE IF NOT EXISTS user ("
                               "username CHAR(50) NULL, "
                               "passwd CHAR(50) NULL) "
                               "ENGINE=InnoDB;";

    if (mysql_query(conn, create_table)) {
        cerr << "❌ Table Creation Failed: " << mysql_error(conn) << endl;
    } else {
        cout << "✅ Table `user` created successfully (or already exists)!" << endl;
    }

    // Insert sample data
    if (mysql_query(conn, "INSERT INTO user(username, passwd) VALUES('name', 'passwd')")) {
        cerr << "❌ Data Insert Failed: " << mysql_error(conn) << endl;
    } else {
        cout << "✅ Data inserted into `user` table!" << endl;
    }

    // Verify data
    if (mysql_query(conn, "SELECT * FROM user")) {
        cerr << "❌ Query Failed: " << mysql_error(conn) << endl;
    } else {
        MYSQL_RES *res = mysql_store_result(conn);
        if (res) {
            cout << "📋 Users in Database `" << database << "`:" << endl;
            MYSQL_ROW row;
            while ((row = mysql_fetch_row(res))) {
                cout << " - Username: " << row[0] << ", Password: " << row[1] << endl;
            }
            mysql_free_result(res);
        }
    }

    // Cleanup
    mysql_close(conn);
    cout << "🔌 Connection closed." << endl;

    return 0;
}
