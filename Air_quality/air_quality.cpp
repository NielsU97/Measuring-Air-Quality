#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <iostream>
#include <chrono>
#include <ctime>
#include <cmath>
#include <thread>
#include <sqlite3.h>
#include <vector>
#include <string>
#include <tuple>
#include "RTIMULib.h"
#include "sgp30.h"

const std::string database = "/homeassistant/database.db";
bool running = true;

class Sensors {
public:
    double temp;
    double hum;
    double press;
    uint16_t eCO2;
    uint16_t TVOC;

    Sensors() : temp(0.0), hum(0.0), press(0.0), eCO2(0), TVOC(0) {}

    void initialize() {

        RTIMUSettings* settings = new RTIMUSettings("RTIMULib");
        imu = RTIMU::createIMU(settings);
        pressure = RTPressure::createPressure(settings);
        humidity = RTHumidity::createHumidity(settings);

        if ((imu == NULL) || (imu->IMUType() == RTIMU_TYPE_NULL)) {
            printf("No IMU found\n");
            exit(1);
        }

        imu->IMUInit();

        if (pressure != NULL)
            pressure->pressureInit();

        if (humidity != NULL)
            humidity->humidityInit();

        SGP30_init("/dev/i2c-1");
        usleep(10000);

        uint64_t serial;
        SGP30_get_serial_id(&serial);
        printf("Serial: 0x%012llx\n", (unsigned long long)serial);

        uint16_t version;
        SGP30_get_feature_set_version(&version);
        printf("Feature set version: 0x%04x\n", version);

        usleep(1000000);
    }

    void readSenseHATData() {
        while (imu->IMURead()) {
            RTIMU_DATA imuData = imu->getIMUData();

            if (pressure != NULL)
                pressure->pressureRead(imuData);

            if (humidity != NULL)
                humidity->humidityRead(imuData);

            temp = imuData.temperature;
            hum = imuData.humidity;
            press = imuData.pressure;
        }

        temp = round(temp * 100) / 100;
        hum = round(hum * 100) / 100;
        press = round(press * 100) / 100;
    }

    void readSGP30Data() {
        SGP30_measure_air_quality(&eCO2, &TVOC);
        //SGP30_measure_raw_signals(&h2, &ethanol);

        eCO2 = round(eCO2 * 100) / 100;
        TVOC = round(TVOC * 100) / 100;
    }

    void cleanup() {
        SGP30_deinit();
        delete imu;
        delete pressure;
        delete humidity;
    }

private:
    RTIMU* imu;
    RTPressure* pressure;
    RTHumidity* humidity;
};

class Time {
public:
    Time() {
        update();
    }

    void update() {
        auto now = std::chrono::system_clock::now();
        time_stamp = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();

        std::time_t time = std::chrono::system_clock::to_time_t(now);
        std::tm* date_time_tm = std::localtime(&time);

        char datetime[20];
        std::strftime(datetime, sizeof(datetime), "%Y-%m-%d %H:%M:%S", date_time_tm);
        date_time = datetime;
    }

    int getTimestamp() const {
        return time_stamp;
    }

    std::string getDateTime() const {
        return date_time;
    }

private:
    int time_stamp;
    std::string date_time;
};

class SQLManager {
public:
    SQLManager(const std::string& dbPath) : db(nullptr) {
        int rc = sqlite3_open(dbPath.c_str(), &db);

        if (rc != SQLITE_OK) {
            std::cerr << "Failed to open database: " << sqlite3_errmsg(db) << std::endl;
            exit(1);
        }
    }

    ~SQLManager() {
        sqlite3_close(db);
    }

    void createSensorsTable() {
        const char* createTableQuery = "CREATE TABLE IF NOT EXISTS sensors(sensor_name VARCHAR(255) PRIMARY KEY UNIQUE, i2c_address VARCHAR(255))";

        char* errMsg;
        int rc = sqlite3_exec(db, createTableQuery, nullptr, nullptr, &errMsg);

        if (rc != SQLITE_OK) {
            std::cerr << "Failed to create 'sensors' table: " << errMsg << std::endl;
            sqlite3_free(errMsg);
            exit(1);
        }
    }

    void insertSensors(const std::string& sensor_name, const std::string& i2c_address) {
        const char* insertQuery = "INSERT OR REPLACE INTO sensors VALUES(?, ?)";

        sqlite3_stmt* stmt;
        int rc = sqlite3_prepare_v2(db, insertQuery, -1, &stmt, nullptr);

        if (rc != SQLITE_OK) {
            std::cerr << "Failed to prepare statement for inserting into 'sensors' table: " << sqlite3_errmsg(db) << std::endl;
            exit(1);
        }

        sqlite3_bind_text(stmt, 1, sensor_name.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 2, i2c_address.c_str(), -1, SQLITE_STATIC);

        rc = sqlite3_step(stmt);
        if (rc != SQLITE_DONE) {
            std::cerr << "Failed to insert into 'sensors' table: " << sqlite3_errmsg(db) << std::endl;
            sqlite3_finalize(stmt);
            exit(1);
        }

        sqlite3_finalize(stmt);
    }

    void createEntitiesTable() {
        const char* createTableQuery = "CREATE TABLE IF NOT EXISTS entities(entity VARCHAR(255) PRIMARY KEY UNIQUE, entity_id VARCHAR(255), sensor_name VARCHAR(255), FOREIGN KEY(sensor_name) REFERENCES sensors(sensor_name))";

        char* errMsg;
        int rc = sqlite3_exec(db, createTableQuery, nullptr, nullptr, &errMsg);

        if (rc != SQLITE_OK) {
            std::cerr << "Failed to create 'entities' table: " << errMsg << std::endl;
            sqlite3_free(errMsg);
            exit(1);
        }
    }

    void insertEntities(const std::string& entity, const std::string& entity_id, const std::string& sensor_name) {
        const char* insertQuery = "INSERT OR REPLACE INTO entities VALUES(?, ?, ?)";

        sqlite3_stmt* stmt;
        int rc = sqlite3_prepare_v2(db, insertQuery, -1, &stmt, nullptr);

        if (rc != SQLITE_OK) {
            std::cerr << "Failed to prepare statement for inserting into 'entities' table: " << sqlite3_errmsg(db) << std::endl;
            exit(1);
        }

        sqlite3_bind_text(stmt, 1, entity.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 2, entity_id.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 3, sensor_name.c_str(), -1, SQLITE_STATIC);

        rc = sqlite3_step(stmt);
        if (rc != SQLITE_DONE) {
            std::cerr << "Failed to insert into 'entities' table: " << sqlite3_errmsg(db) << std::endl;
            sqlite3_finalize(stmt);
            exit(1);
        }

        sqlite3_finalize(stmt);
    }

    void createMetricsTable() {
        const char* createTableQuery = "CREATE TABLE IF NOT EXISTS metrics(entity VARCHAR(255), time_stamp TIMESTAMP, date_time DATETIME, value DOUBLE(20), unit_of_measurement VARCHAR(255), PRIMARY KEY (entity, time_stamp), FOREIGN KEY(entity) REFERENCES entities(entity))";

        char* errMsg;
        int rc = sqlite3_exec(db, createTableQuery, nullptr, nullptr, &errMsg);

        if (rc != SQLITE_OK) {
            std::cerr << "Failed to create 'metrics' table: " << errMsg << std::endl;
            sqlite3_free(errMsg);
            exit(1);
        }
    }

    void insertMetrics(const std::string& entity, int time_stamp, const std::string& date_time, double value, const std::string& unit) {
        const char* insertQuery = "INSERT INTO metrics(entity, time_stamp, date_time, value, unit_of_measurement) VALUES(?, ?, ?, ?, ?)";

        sqlite3_stmt* stmt;
        int rc = sqlite3_prepare_v2(db, insertQuery, -1, &stmt, nullptr);

        if (rc != SQLITE_OK) {
            std::cerr << "Failed to prepare statement for inserting into 'metrics' table: " << sqlite3_errmsg(db) << std::endl;
            exit(1);
        }

        sqlite3_bind_text(stmt, 1, entity.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_int(stmt, 2, time_stamp);
        sqlite3_bind_text(stmt, 3, date_time.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_double(stmt, 4, value);
        sqlite3_bind_text(stmt, 5, unit.c_str(), -1, SQLITE_STATIC);

        rc = sqlite3_step(stmt);
        if (rc != SQLITE_DONE) {
            std::cerr << "Failed to insert into 'metrics' table: " << sqlite3_errmsg(db) << std::endl;
            sqlite3_finalize(stmt);
            exit(1);
        }

        sqlite3_finalize(stmt);
    }

    void deleteMetrics(int limit) {
        std::string deleteQuery = "DELETE FROM metrics WHERE ROWID IN (SELECT ROWID FROM metrics ORDER BY ROWID DESC LIMIT -1 OFFSET " + std::to_string(limit) + ")";
        char* errMsg = nullptr;
        int rc = sqlite3_exec(db, deleteQuery.c_str(), nullptr, nullptr, &errMsg);
        if (rc != SQLITE_OK) {
            std::cerr << "Failed to delete rows from 'metrics' table: " << errMsg << std::endl;
            sqlite3_free(errMsg);
        }
    }

private:
    sqlite3* db;
};


void signalHandler(int signal) {
    if (signal == SIGINT) {
        running = false;
        std::cout << "" << std::endl;
        std::cout << "============================" << std::endl;
        std::cout << "Program terminated by user" << std::endl;
        std::cout << "Busy with terminating the program..." << std::endl;
    }
}

int main() {
    signal(SIGINT, signalHandler);

    Sensors sensor;
    Time time;
    SQLManager sqlManager(database);

    sensor.initialize();
    sqlManager.createSensorsTable();
    sqlManager.createEntitiesTable();
    sqlManager.createMetricsTable();

    std::vector<std::pair<std::string, std::string>> sensors = {
        {"HTS221", "0x5f"},
        {"LPS25H", "0x5c"},
        {"SGP30", "0x58"}
    };

    std::vector<std::tuple<std::string, std::string, std::string>> entities = {
        {"Temperature", "sensor.sensehat_temp", "HTS221"},
        {"Humidity", "sensor.sensehat_hum", "HTS221"},
        {"Pressure", "sensor.sensehat_press", "LPS25H"},
        {"CO2eq", "sensor.sgp30_CO2eq", "SGP30"},
        {"TVOC", "sensor.sgp30_TVOC", "SGP30"}
    };

    for (const auto& sensor : sensors) {
        std::string sensor_name = std::get<0>(sensor);
        std::string i2c_address = std::get<1>(sensor);
        sqlManager.insertSensors(sensor_name, i2c_address);
    }

    for (const auto& entity : entities) {
        std::string entity_name = std::get<0>(entity);
        std::string entity_id = std::get<1>(entity);
        std::string sensor_name = std::get<2>(entity);
        sqlManager.insertEntities(entity_name, entity_id, sensor_name);
    }

    while (running) {
        sensor.readSenseHATData();
        sensor.readSGP30Data();

        double temperature = sensor.temp;
        double humidity = sensor.hum;
        double pressure = sensor.press;
        uint16_t eCO2 = sensor.eCO2;
        uint16_t TVOC = sensor.TVOC;

        time.update();

        std::vector<std::tuple<std::string, int, std::string, double, std::string>> data = {
            {"Temperature", time.getTimestamp(), time.getDateTime(), temperature, "°C"},
            {"Humidity", time.getTimestamp(), time.getDateTime(), humidity , "%RH"},
            {"Pressure", time.getTimestamp(), time.getDateTime(), pressure, "Pa"},
            {"CO2eq", time.getTimestamp(), time.getDateTime(), eCO2, "ppm"},
            {"TVOC", time.getTimestamp(), time.getDateTime(), TVOC, "ppb"}
        }; 
        
        for (const auto& metric : data) {
            std::string metricEntity = std::get<0>(metric);
            int metricTimestamp = std::get<1>(metric);
            std::string metricDateTime = std::get<2>(metric);
            double metricValue = std::get<3>(metric);
            std::string metricUnit = std::get<4>(metric);

            sqlManager.insertMetrics(metricEntity, metricTimestamp, metricDateTime, metricValue, metricUnit);
        }

        sqlManager.deleteMetrics(180);

        std::this_thread::sleep_for(std::chrono::minutes(1));
    }

    sensor.cleanup();
    std::cout << "Program succesfully terminated" << std::endl;
    return 0;
}