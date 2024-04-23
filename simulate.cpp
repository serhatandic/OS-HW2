#include <iostream>
#include <vector>
#include <utility>
#include <pthread.h>
#include <string>
#include <tuple>
#include "monitor.h"

class Connector: public Monitor {
public:
    int id;
    int travelTime;
    int maxWaitTime;
    char type;
    int capacity;
    int currentCount;
    std::vector<Condition*> conditions;
    
    Connector(int id, int travelTime, int maxWaitTime, char type, int capacity = 0)
        : id(id), travelTime(travelTime), maxWaitTime(maxWaitTime), type(type), capacity(capacity), currentCount(0) {
        // Initialize conditions based on the type of connector
        conditions.push_back(new Condition(this)); // General condition for passing
        if (type == 'F' || type == 'C') {
            conditions.push_back(new Condition(this)); // Additional conditions for ferries or crossroads
        }
    }

};

class Car: public Monitor {
public:
    int id;
    int travelTime;

    std::vector<std::tuple<char, int, int, int>> path; // connector type, connector ID, from, to
    Car(int id, int travelTime) : id(id), travelTime(travelTime) {}
};

std::vector<Connector> connectors;
std::vector<Car> cars;

void initializeConnectorsAndCars() {
    int numNarrowBridges, numFerries, numCrossroads;
    int travelTime, maxWaitTime, capacity;
    char connectorType;

    // Reading narrow bridges
    std::cin >> numNarrowBridges;
    for (int i = 0; i < numNarrowBridges; ++i) {
        std::cin >> travelTime >> maxWaitTime;
        connectors.push_back(Connector(i, travelTime, maxWaitTime, 'N'));
    }

    // Reading ferries
    std::cin >> numFerries;
    for (int i = 0; i < numFerries; ++i) {
        std::cin >> travelTime >> maxWaitTime >> capacity;
        connectors.push_back(Connector(i + numNarrowBridges, travelTime, maxWaitTime, 'F', capacity));
    }

    // Reading crossroads
    std::cin >> numCrossroads;
    for (int i = 0; i < numCrossroads; ++i) {
        std::cin >> travelTime >> maxWaitTime;
        connectors.push_back(Connector(i + numNarrowBridges + numFerries, travelTime, maxWaitTime, 'C'));
    }

    // Reading cars
    int numCars, pathLength;
    std::string connector;

    std::cin >> numCars;
    
    for (int i = 0; i < numCars; ++i) {
        std::cin >> travelTime >> pathLength;
        Car newCar(i, travelTime);
        for (int j = 0; j < pathLength; ++j) {
            int from, to;
            std::cin >> connector >> from >> to;

            char type = connector[0]; // This is 'N', 'F', or 'C'
            int connectorID = std::stoi(connector.substr(1)); // This extracts the ID part
            
            newCar.path.push_back(std::make_tuple(type, connectorID, from, to));
        }
        cars.push_back(newCar);
    }

    // test
    for (auto& car: cars) {
        std::cout << "\n-----\n";
        std::cout << car.id << " " << car.travelTime << std::endl;
        for (auto& path: car.path) {
            std::cout << std::get<0>(path) << " " << std::get<1>(path) << " " << std::get<2>(path) << " " << std::get<3>(path) << std::endl;
        }
    }
};

int main() {
    initializeConnectorsAndCars();
}