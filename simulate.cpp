#include <iostream>
#include <vector>
#include <utility>
#include <pthread.h>
#include <string>
#include <map>
#include <tuple>
#include <string>
#include <queue>
#include <time.h>

#include "monitor.h"
#include "helper.h"
#include "WriteOutput.h"

class Car{
public:
    int id;
    int travelTime;
    std::vector<std::tuple<char, int, int, int>> path; // connector type, connector ID, from, to

    Car(int id, int travelTime) : id(id), travelTime(travelTime) {}
};

class NarrowBridge: public Monitor {
public:
    int id;
    int travelTime;
    int maxWaitTime;

    int direction = 0;

    std::vector<std::queue<Car*>> carsInLine = {std::queue<Car*>(), std::queue<Car*>()};

    pthread_mutex_t  mut2;

    Condition* lineCondition = new Condition(this);
    Condition* directionCondition = new Condition(this);

    Condition* timedwait = new Condition(this);
    struct timespec ts;

    NarrowBridge(int id, int travelTime, int maxWaitTime)
        : id(id), travelTime(travelTime), maxWaitTime(maxWaitTime) {
            pthread_mutex_init(&mut2, NULL);
            clock_gettime(CLOCK_REALTIME, &ts);
            ts.tv_sec += this->maxWaitTime;
    }

    void pass(int from, int to, Car* car) {
        __synchronized__;

        int connectorID = this->id;
        char connectorType = 'N';

        bool carPassedBefore = false;

        
        
        while(true){
            // if the directions are the same, check if the bridge is empty. If the bridge is empty check if there are other cars waiting (who arrived before)
            if (this->direction == to) {
                if (!carsInLine.empty() && carsInLine[to].front() == car){
                    // can pass, Sleep for PASS_DELAY milliseconds if a car has passed before
                    if (carPassedBefore) {
                        sleep_milli(PASS_DELAY);
                        carPassedBefore = false;
                    }
                    WriteOutput(car->id, connectorType, connectorID, START_PASSING);
                    lineCondition->notifyAll();
                    sleep_milli(travelTime);
                    WriteOutput(car->id, connectorType, connectorID, FINISH_PASSING);
                    // remove the car from the line
                    this->carsInLine[to].pop();
                    break;
                }else {
                    std::cout << "Car " << car->id << " is waiting for the bridge to be empty" << std::endl;
                    // not the first car in line, wait for the car to pass
                    carPassedBefore = true;
                    std::cout << "Car " << car->id << " is waiting for the car in front to pass" << std::endl;
                    lineCondition->wait();
                }
            }else if (timedwait->timedwait(&ts) == ETIMEDOUT || this->carsInLine[from].empty()){
                direction = !direction;
                directionCondition->notifyAll();
                // every time the direction is updated, start a new timer. If the timer reaches the maxWaitTime, change the direction
                clock_gettime(CLOCK_REALTIME, &ts);
                ts.tv_sec += this->maxWaitTime;
            }else{
                std::cout << "Car " << car->id << " is waiting for the direction to change" << std::endl;
                directionCondition->wait(); 
            }
        }
        
    }

    ~NarrowBridge() {
    }
};

class Ferry: public Monitor {
public:
    int id;
    int travelTime;
    int maxWaitTime;
    int capacity;
    char type = 'F';

    Condition* carPassing;
    Condition* carInLine;
    Condition* direction;

    Ferry(int id, int travelTime, int maxWaitTime, int capacity)
        : id(id), travelTime(travelTime), maxWaitTime(maxWaitTime), capacity(capacity) {
        carPassing = new Condition(this);
        carInLine = new Condition(this);
        direction = new Condition(this);
    }

    void pass(int from, int to, Car* car) {
        // Implement this function
    }

    ~Ferry() {
        delete carPassing;
        delete carInLine;
        delete direction;
    }
};

class Crossroad: public Monitor {
public:
    int id;
    int travelTime;
    char type = 'C';
    std::vector<int> maxWaitTime;

    Condition* carPassing;
    Condition* carInLine;
    Condition* direction;

    Crossroad(int id, int travelTime, int maxWaitTime0, int maxWaitTime1, int maxWaitTime2, int maxWaitTime3)
        : id(id), travelTime(travelTime), maxWaitTime(std::vector<int>{maxWaitTime0, maxWaitTime1, maxWaitTime2, maxWaitTime3}) {
        carPassing = new Condition(this);
        carInLine = new Condition(this);
        direction = new Condition(this);
    }

    void pass(int from, int to, Car* car) {
        // Implement this function
    }

    ~Crossroad() {
        delete carPassing;
        delete carInLine;
        delete direction;
    }
};



std::map<int, Monitor*> connectorMap;
std::vector<Car> cars;

void initializeConnectorsAndCars() {
    int numNarrowBridges, numFerries, numCrossroads;
    int travelTime, maxWaitTime, capacity;

    // Reading narrow bridges
    std::cin >> numNarrowBridges;
    for (int i = 0; i < numNarrowBridges; ++i) {
        std::cin >> travelTime >> maxWaitTime;
        connectorMap[i] = dynamic_cast<Monitor*>(new NarrowBridge(i, travelTime, maxWaitTime));
        
    }

    // Reading ferries
    std::cin >> numFerries;
    for (int i = 0; i < numFerries; ++i) {
        std::cin >> travelTime >> maxWaitTime >> capacity;
        connectorMap[i + numNarrowBridges] = dynamic_cast<Monitor*>(new Ferry(i + numNarrowBridges, travelTime, maxWaitTime, capacity));
    }

    // Reading crossroads
    std::cin >> numCrossroads;
    for (int i = 0; i < numCrossroads; ++i) {
        std::cin >> travelTime >> maxWaitTime;
        connectorMap[i + numNarrowBridges + numFerries] = dynamic_cast<Monitor*>(new Crossroad(i + numNarrowBridges + numFerries, travelTime, maxWaitTime, maxWaitTime, maxWaitTime, maxWaitTime));
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
};

void* carThreadRoutine(void* arg) {
    Car* car = static_cast<Car*>(arg);
    for (const auto& step : car->path) {
        char type = std::get<0>(step);
        int connectorID = std::get<1>(step);
        int from = std::get<2>(step);
        int to = std::get<3>(step);
        
        switch (type) {
            case 'N':{
                NarrowBridge* conn = dynamic_cast<NarrowBridge*>(connectorMap[connectorID]);

                int travelTime = car->travelTime;

                WriteOutput(car->id, 'N', conn->id, TRAVEL);
                sleep_milli(travelTime);
                pthread_mutex_lock(&conn->mut2);
                conn->carsInLine[to].push(car);
                WriteOutput(car->id, 'N', conn->id, ARRIVE);
                pthread_mutex_unlock(&conn->mut2);
                conn->pass(from, to, car);
                break;
            }
                
            case 'F':{
                Ferry* conn = dynamic_cast<Ferry*>(connectorMap[connectorID]);

                int travelTime = car->travelTime;

                WriteOutput(car->id, 'N', conn->id, TRAVEL);
                sleep_milli(travelTime);
                WriteOutput(car->id, 'N', conn->id, ARRIVE);
                conn->pass(from, to, car);
                break;
            }
                
            case 'C':{
                Crossroad* conn = dynamic_cast<Crossroad*>(connectorMap[connectorID]);
                
                int travelTime = car->travelTime;

                WriteOutput(car->id, 'N', conn->id, TRAVEL);
                sleep_milli(travelTime);
                WriteOutput(car->id, 'N', conn->id, ARRIVE);
                conn->pass(from, to, car);
                break;
            }
                
        }
    }
    return nullptr;
}





int main() {
    InitWriteOutput();
    initializeConnectorsAndCars();

    std::vector<pthread_t> threads(cars.size());
    for (size_t i = 0; i < cars.size(); ++i) {
        pthread_create(&threads[i], nullptr, carThreadRoutine, &cars[i]);
    }

    for (auto& thread : threads) {
        pthread_join(thread, nullptr);
    }

    // Clean up
    for (auto& connector : connectorMap) {
        delete connector.second;
    }
}