Designing a SQLite Database {#mainpage}
=========

This report presents the development of a C++ program that creates a SQLite database and stores measurement data in it. The program is designed to interact with home automation software, specifically Home Assistant, to display the stored data.

**DTB documentation
Embedded Systems Engineering**
- Niels Urgert

## General Information

**Institution**
High School: Hogeschool van Arnhem en Nijmegen (HAN)
Faculty: Academic Engineering and Automotive (AEA)
Education: Embedded Systems Engineering (ESE)

## Summary

The goal of this project was to design and implement a SQLite database for storing measurement data. The program utilizes the SQLite library to interact with the database and perform operations such as creating tables, inserting data, and querying the stored data. The stored measurement data can then be accessed and displayed in Home Assistant using the appropriate integrations.

## Conceptual database design 
The conceptual database design is represented by an Entity-Relationship (ER) diagram. The diagram illustrates the relationships between different entities and their attributes. In this case, the diagram depicts the relationships between sensors, entities, and metrics. Each sensor is associated with multiple entities, and each entity corresponds to multiple metrics.

<img src="https://github.com/NielsU97/Measering-Air-Quality/blob/main/Documentation/images/ERdiagram.png" width="300">

## Logical database design 
The logical database design defines the structure of the database tables and their attributes. The tables include 'sensors,' 'entities,' and 'metrics.' The 'sensors' table stores information about the sensors, such as their names and addresses. The 'entities' table represents the entities measured by the sensors, along with their corresponding sensor names. The 'metrics' table contains the actual measurement data, including the entity, timestamp, datetime, value, and unit.

<img src="https://github.com/NielsU97/Measering-Air-Quality/blob/main/Documentation/images/LogicalDesign.png" width="500">

## Dashboard
To visualize and interact with the stored data, a dashboard is implemented using Home Assistant. 

<img src="https://github.com/NielsU97/Measering-Air-Quality/blob/main/Documentation/images/GUI.png" width="500">

<img src="https://github.com/NielsU97/Measering-Air-Quality/blob/main/Documentation/images/GUI_2.png" width="500">

## Implementation
The database is implemented in Home Assistant. It uses the SQL database integration, which makes it possible to read out databases using queries. 

<img src="https://github.com/NielsU97/Measering-Air-Quality/blob/main/Documentation/images/Integration.png" width="600">

The figure below shows the working implementation. The most recent data is displayed in Home Assistant and matches the correct value in the database.

<img src="https://github.com/NielsU97/Measering-Air-Quality/blob/main/Documentation/images/DatabaseGUI.png" width="600">

## Appendix A: Configuration Dashboard

name:	SQL Temperature
database: sqlite:////config/database.db
query: SELECT "value" FROM "metrics" WHERE entity = "Temperature" ORDER BY "time_stamp" DESC LIMIT 1;
column: 	value
Unit: °C

name: SQL Humidity
database: sqlite:////config/database.db
query: SELECT "value" FROM "metrics" WHERE entity = "Humidity" ORDER BY "time_stamp" DESC LIMIT 1;
column: 	value
Unit: %

name: SQL Pressure
database: sqlite:////config/database.db
query: SELECT "value" FROM "metrics" WHERE entity = "Pressure" ORDER BY "time_stamp" DESC LIMIT 1;
column: 	value
Unit: Pa

name: SQL CO2eq
database: sqlite:////config/database.db
query: SELECT "value" FROM "metrics" WHERE entity = "CO2eq" ORDER BY "time_stamp" DESC LIMIT 1;
column: 	value
Unit: ppm

name: SQL TVOC
database: sqlite:////config/database.db
query: SELECT "value" FROM "metrics" WHERE entity = "TVOC" ORDER BY "time_stamp" DESC LIMIT 1;
column: 	value
Unit: ppb

