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
    int direction = -1;
    bool shouldBePassDelay = false;

    struct timespec lastSwitchTime;

    std::vector<int> numOfCarsMovingTo = {0, 0};
    std::vector<std::queue<Car*>> carsInLine = {std::queue<Car*>(), std::queue<Car*>()};

    pthread_mutex_t  mut2;

    Condition* lineCondition = new Condition(this);
    Condition* directionCondition = new Condition(this);
    Condition* sleepCondition = new Condition(this);
    Condition* waitReversePassingCars = new Condition(this);

    NarrowBridge(int id, int travelTime, int maxWaitTime)
        : id(id), travelTime(travelTime), maxWaitTime(maxWaitTime) {
            pthread_mutex_init(&mut2, NULL);
    }

    void pass(int from, int to, Car* car) {
        pthread_mutex_lock(&mut2);

        carsInLine[to].push(car);
        WriteOutput(car->id, 'N', this->id, ARRIVE);

        pthread_mutex_unlock(&mut2);
        __synchronized__;

        if (direction == -1) {
            direction = to;
        }

        int connectorID = this->id;
        char connectorType = 'N';        

        while(true){
            if (this->direction == to) {
                if (numOfCarsMovingTo[from] > 0){ // there was a car on the bridge during the direction switch, wait for it to pass
                        waitReversePassingCars->wait();
                }else if (carsInLine[to].front() == car){
                    if (shouldBePassDelay){
                        struct timespec tmp_ts;
                        clock_gettime(CLOCK_REALTIME, &tmp_ts);
                        tmp_ts.tv_sec += PASS_DELAY / 1000;
                        tmp_ts.tv_nsec += (PASS_DELAY % 1000) * 1000000;
                        sleepCondition->timedwait(&tmp_ts);

                        if (direction != to){
                            lineCondition->notifyAll();
                            continue;
                        }
                    }
                    shouldBePassDelay = true;
                    
                    WriteOutput(car->id, connectorType, connectorID, START_PASSING);
                    lineCondition->notifyAll();

                    this->carsInLine[to].pop();
                    numOfCarsMovingTo[to]++;

                    break;
                }else {
                    lineCondition->wait();
                }
            }else if(carsInLine[from].empty() && numOfCarsMovingTo[from] == 0){
                directionCondition->notifyAll();
                direction = !direction;
                shouldBePassDelay = false;
            }else{
                timespec ts;
                clock_gettime(CLOCK_REALTIME, &ts);
                ts.tv_sec += maxWaitTime / 1000;
                ts.tv_nsec += (maxWaitTime % 1000) * 1000000;
  
                int result = directionCondition->timedwait(&ts);

                if(direction != to) {
                    if(result == ETIMEDOUT) {
                        directionCondition->notifyAll();
                    }
                    direction = !direction;
                    shouldBePassDelay = false;
                }
                
            }
        }
        
    }

    void finishPassing(Car* car, int from, int to) {
        sleep_milli(travelTime);
        __synchronized__;
        
        WriteOutput(car->id, 'N', this->id, FINISH_PASSING);
        numOfCarsMovingTo[to]--;
        if (numOfCarsMovingTo[to] == 0){
            if (carsInLine[to].empty()){
                directionCondition->notifyAll();
            }
            waitReversePassingCars->notifyAll();
        }
    }

    ~NarrowBridge() {
        delete lineCondition;
        delete directionCondition;
    }
};

class Ferry: public Monitor {
public:
    int id;
    int travelTime;
    int maxWaitTime;
    int capacity = 0;
    char type = 'F';

    int numOfCars = 0;
    Condition* start_passing_condition = new Condition(this);
    Condition* finish_passing_condition = new Condition(this);
    Condition* sleepCondition = new Condition(this);

    Ferry(int id, int travelTime, int maxWaitTime, int capacity)
        : id(id), travelTime(travelTime), maxWaitTime(maxWaitTime), capacity(capacity) {
    }

    void pass(int from, int to, Car* car) {
        __synchronized__;
        numOfCars++;
        WriteOutput(car->id, type, this->id, ARRIVE);
        if (numOfCars == capacity){
            WriteOutput(car->id, type, this->id, START_PASSING);
            for (int i = 0; i < capacity - 1; i++){
                start_passing_condition->notifyAll();
            }
        }else{
            timespec ts;
            clock_gettime(CLOCK_REALTIME, &ts);
            ts.tv_sec += maxWaitTime / 1000;
            ts.tv_nsec += (maxWaitTime % 1000) * 1000000;

            start_passing_condition->timedwait(&ts);
            // notify everyone else
            for (int i = 0; i < numOfCars - 1; i++){
                start_passing_condition->notifyAll();
            }
            WriteOutput(car->id, type, this->id, START_PASSING);
        }
        numOfCars = 0;
    }

    void finishPassing(Car* car, int from, int to) {
        sleep_milli(travelTime);
        __synchronized__;
        WriteOutput(car->id, type, this->id, FINISH_PASSING);
    }

    ~Ferry() {
        delete start_passing_condition;
        delete finish_passing_condition;
    }
};

class Crossroad: public Monitor {
public:
    int id;
    int travelTime;
    char type = 'C';
    int maxWaitTime;

    int direction = -1;
    bool shouldBePassDelay = false;

    struct timespec lastSwitchTime;

    std::vector<int> numOfCarsMovingFrom = {0, 0, 0, 0};
    std::vector<std::queue<Car*>> carsInLine = {std::queue<Car*>(), std::queue<Car*>(), std::queue<Car*>(), std::queue<Car*>()};

    pthread_mutex_t  mut2;

    Condition* lineCondition = new Condition(this);
    std::vector<Condition*> directionConditions = {new Condition(this), new Condition(this), new Condition(this), new Condition(this)};
    Condition* sleepCondition = new Condition(this);
    Condition* waitReversePassingCars = new Condition(this);
    
    Crossroad(int id, int travelTime, int maxWaitTime)
        : id(id), travelTime(travelTime), maxWaitTime(maxWaitTime) {
        pthread_mutex_init(&mut2, NULL);
    }

    void pass(int from, Car* car) {
        pthread_mutex_lock(&mut2);

        carsInLine[from].push(car);
        WriteOutput(car->id, 'C', this->id, ARRIVE);

        pthread_mutex_unlock(&mut2);
        __synchronized__;

        if (direction == -1) {
            direction = from;
        }

        int connectorID = this->id;
        char connectorType = 'C';        

        while(true){
            if (this->direction == from) {
                // there was a car on the bridge during the direction switch, wait for it to pass
                // check all directions except the current one 
                if (numOfCarsMovingFrom[(from + 1) % 4] > 0 || numOfCarsMovingFrom[(from + 2) % 4] > 0 || numOfCarsMovingFrom[(from + 3) % 4] > 0){ 
                    waitReversePassingCars->wait();
                }else if (carsInLine[from].front() == car){
                    if (shouldBePassDelay){
                        struct timespec tmp_ts;
                        clock_gettime(CLOCK_REALTIME, &tmp_ts);
                        tmp_ts.tv_sec += PASS_DELAY / 1000;
                        tmp_ts.tv_nsec += (PASS_DELAY % 1000) * 1000000;
                        sleepCondition->timedwait(&tmp_ts);

                        // there was a direction change during the delay
                        if (direction != from){
                            lineCondition->notifyAll();
                            continue;
                        }
                    }
                    // shouldBePassDelay = true;
                    
                    WriteOutput(car->id, connectorType, connectorID, START_PASSING);
                    lineCondition->notifyAll();

                    this->carsInLine[from].pop();
                    numOfCarsMovingFrom[from]++;

                    break;
                }else {
                    struct timespec tmp_ts;
                    clock_gettime(CLOCK_REALTIME, &tmp_ts);
                    tmp_ts.tv_sec += PASS_DELAY / 1000;
                    tmp_ts.tv_nsec += (PASS_DELAY % 1000) * 1000000;
                    int res = lineCondition->timedwait(&tmp_ts);
                    if (res == ETIMEDOUT){
                        shouldBePassDelay = false;
                    }else{
                        shouldBePassDelay = true;
                    }
                }
                // on the next else if block, we should check 
                // if not we can change the direction
            }else if(carsInLine[direction].empty() && numOfCarsMovingFrom[direction] == 0){
                // • Next active direction is determined by incrementing numbers in a circular way. Next active direction's waiting line shouldn't be empty. If empty the direction should be changed again.
                // should notify the next direction
                while (carsInLine[direction].empty() && numOfCarsMovingFrom[direction] == 0){
                    direction = (direction + 1) % 4;
                }
                directionConditions[direction]->notifyAll();
                shouldBePassDelay = false;
            }else{
                timespec ts;
                clock_gettime(CLOCK_REALTIME, &ts);
                ts.tv_sec += maxWaitTime / 1000;
                ts.tv_nsec += (maxWaitTime % 1000) * 1000000;
  
                int result = directionConditions[from]->timedwait(&ts);

                if(direction != from) {
                    if(result == ETIMEDOUT) {
                        while (carsInLine[direction].empty()){
                            direction = (direction + 1) % 4;
                        }
                        directionConditions[direction]->notifyAll();
                    }
                    shouldBePassDelay = false;
                }
                
            }
        }
        
    }

    void finishPassing(Car* car, int from) {
        sleep_milli(travelTime);
        __synchronized__;
        
        WriteOutput(car->id, 'C', this->id, FINISH_PASSING);
        numOfCarsMovingFrom[from]--;
        if (numOfCarsMovingFrom[from] == 0){
            if (carsInLine[from].empty()){
                while (carsInLine[direction].empty()){
                    direction = (direction + 1) % 4;
                    // check if i am the last car of all directions
                    if (carsInLine[(direction + 1) % 4].empty() && carsInLine[(direction + 2) % 4].empty() && carsInLine[(direction + 3) % 4].empty()){
                        break;
                    }
                }
                directionConditions[direction]->notifyAll();
            }
            waitReversePassingCars->notifyAll();
        }
    }

    ~Crossroad() {
        delete lineCondition;
        for (auto condition : directionConditions) {
            delete condition;
        }
    }
};



std::map<char, std::vector<Monitor*>> connectorMap;
std::vector<Car> cars;

void initializeConnectorsAndCars() {
    int numNarrowBridges, numFerries, numCrossroads;
    int travelTime, maxWaitTime, capacity;

    // Reading narrow bridges
    std::cin >> numNarrowBridges;
    for (int i = 0; i < numNarrowBridges; ++i) {
        std::cin >> travelTime >> maxWaitTime;
        connectorMap['N'].push_back(dynamic_cast<Monitor*>(new NarrowBridge(i, travelTime, maxWaitTime)));
        
    }

    // Reading ferries
    std::cin >> numFerries;
    for (int i = 0; i < numFerries; ++i) {
        std::cin >> travelTime >> maxWaitTime >> capacity;
        connectorMap['F'].push_back(dynamic_cast<Monitor*>(new Ferry(i + numNarrowBridges, travelTime, maxWaitTime, capacity)));
    }

    // Reading crossroads
    std::cin >> numCrossroads;
    for (int i = 0; i < numCrossroads; ++i) {
        std::cin >> travelTime >> maxWaitTime;
        connectorMap['C'].push_back(dynamic_cast<Monitor*>(new Crossroad(i + numNarrowBridges + numFerries, travelTime, maxWaitTime)));
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
        WriteOutput(car->id, type, connectorID, TRAVEL);
        sleep_milli(car->travelTime);
        switch (type) {
            case 'N':{
                NarrowBridge* conn = dynamic_cast<NarrowBridge*>(connectorMap['N'][connectorID]);

                conn->pass(from, to, car);
                conn->finishPassing(car, from, to);
                break;
            }
                
            case 'F':{
                Ferry* conn = dynamic_cast<Ferry*>(connectorMap['F'][connectorID]);

                conn->pass(from, to, car);
                conn->finishPassing(car, from, to);
                break;
            }
                
            case 'C':{
                Crossroad* conn = dynamic_cast<Crossroad*>(connectorMap['C'][connectorID]);
            
                conn->pass(from, car);
                conn->finishPassing(car, from);
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
    for (auto& connectorType : connectorMap) {
        for (auto& connector : connectorType.second) {
            delete connector;
        }
    }
}