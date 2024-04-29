#include <iostream>
#include <vector>
#include <cstring>
#include <string>
#include "WriteOutput.h"
#include "helper.h"
#include "monitor.h"
#include "pthread.h"
#include <queue>
#include <chrono>
#include <csignal>

using namespace std;

struct Car {
    int travelTime;
    int pathLength;
    vector<pair<string, pair<int, int>>> pathObjects;
};

// FIX TIMEOUT
class NarrowBridgeMonitor : public Monitor {
    Condition waiting, directionWait, waitPassingCarsInReverse, tmp; //0,1   1,0 
    // use one more condition variable other than waiting
    // in order to make the code faster

    vector<queue<int>> dirqueues = {queue<int>(), queue<int>()};
    pthread_mutex_t mut2; // for queue operations
    struct timespec ts;
    
    int currentlyPassings[2] = {0, 0};
    bool delayForPassing = false;
    int shallIWait = false;

public:
    const int travelTime;
    const int maxWaitTime;
    int direction;

    NarrowBridgeMonitor(int travelT, int maxWaitT) : travelTime(travelT), maxWaitTime(maxWaitT) 
    , waiting(this), directionWait(this), waitPassingCarsInReverse(this), tmp(this) {
        direction = -1;
    }

    void pass(int carId, int connectorId, int dir) {
        
        pthread_mutex_lock(&mut2);

        WriteOutput(carId, 'N', connectorId, ARRIVE);

     
        // no 0,0 or 1,1 for this case
        
        dirqueues[dir].push(carId);
        
        
        if(direction == -1) {
            direction = dir;
        }
        pthread_mutex_unlock(&mut2);
        __synchronized__;

        while(true) {
            // first car determines the direction
            //check direction
            
            if(direction == dir) {
            //check if you are first
                if(currentlyPassings[!dir] > 0) { // car passing in reverse order
                    waitPassingCarsInReverse.wait();
                }

                else if(delayForPassing) {
                    waiting.wait();
                }

                else if(dirqueues[dir].front() == carId) {
                    //pass
                    
        
                    
                    if(shallIWait) {
                        struct timespec tmp_ts;
                        clock_gettime(CLOCK_REALTIME, &tmp_ts);
                        tmp_ts.tv_sec += PASS_DELAY / 1000;
                        tmp_ts.tv_nsec += (PASS_DELAY % 1000) * 1000000;
                        delayForPassing = true;

                        tmp.timedwait(&tmp_ts); // it should not wait if there is no car before it

                        if(direction != dir) { // if direction changes, at this point you should leave
                            // direction switched by timeout
                            delayForPassing = false;
                            waiting.notifyAll(); // they should now wait for directionWait
                            continue;

                        }
                        
                    }
                    shallIWait = true;

                    currentlyPassings[dir]++;
                        
                    WriteOutput(carId, 'N', connectorId, START_PASSING);
                    
                    pthread_mutex_lock(&mut2);
                    dirqueues[dir].pop();
                    pthread_mutex_unlock(&mut2);
                    delayForPassing = false;

                    waiting.notifyAll();
                    
                    break;

                }
                else {
                    waiting.wait();
                
                }

            }

            else if(dirqueues[!dir].empty() && currentlyPassings[!dir] == 0) { 
                //there are no more cars in the other direction, toggle directions
                directionWait.notifyAll(); 
                direction = !direction;
                shallIWait = false;
            }

            else { // wait for direction to change
                // may not be healthy for all to use timedwait

                clock_gettime(CLOCK_REALTIME, &ts);
                ts.tv_sec += maxWaitTime / 1000;
                ts.tv_nsec += (maxWaitTime % 1000) * 1000000;
                int result = directionWait.timedwait(&ts);
                
                if(direction != dir) {
                    if(result == ETIMEDOUT) {
                        directionWait.notifyAll();
                    }
                    direction = !direction;
                    shallIWait = false;
                    cout << "new direction:" << direction << endl;
                }
            }
        }
    }
    void finishPass(int carId, int connectorId, int dir) {
        sleep_milli(travelTime);
        __synchronized__;
    
        
        if(--currentlyPassings[dir] == 0) {
            if(dirqueues[dir].empty()) {
                directionWait.notifyAll();
            }
            waitPassingCarsInReverse.notifyAll();
            
        }
        
        WriteOutput(carId, 'N', connectorId, FINISH_PASSING);
    }
};

class FerryMonitor : public Monitor {
    // Condition cv1, cv2;
public:    
    const int travelTime;
    const int maxWaitTime;
    const int capacity;

    FerryMonitor(int travelT, int maxWaitT, int c) : travelTime(travelT), maxWaitTime(maxWaitT), 
    capacity(c) /* cv1(this), cv2(this)*/ {

    }
    void pass() {
        __synchronized__;
        // TODO
    }
};

class CrossRoadMonitor : public Monitor {
    // Condition cv1, cv2;
public:
    const int travelTime;
    const int maxWaitTime;

    CrossRoadMonitor(int travelT, int maxWaitT) : travelTime(travelT), maxWaitTime(maxWaitT) 
    /* cv1(this), cv2(this)*/ {

    }
    void pass() {
        __synchronized__;
        // TODO
    }
};

struct ThreadArgs {
    int carIndex;
    Car carData;
    vector<CrossRoadMonitor*> crossRoads;
    vector<FerryMonitor*> ferries;
    vector<NarrowBridgeMonitor*> narrowBridges;
};

void* carThread(void* arg) {
    ThreadArgs* args = (ThreadArgs*) arg;
    int carIndex = args->carIndex;
    Car carData = args->carData;
    vector<CrossRoadMonitor*> crossRoads = args->crossRoads;
    vector<FerryMonitor*> ferries = args->ferries;
    vector<NarrowBridgeMonitor*> narrowBridges = args->narrowBridges;
    
    delete args;

    
    for(int i=0; i<carData.pathLength; ++i) {   
        char currentConnectorType = carData.pathObjects[i].first[0];
        int currentConnectorID = std::stoi(carData.pathObjects[i].first.substr(1));

        WriteOutput(carIndex, currentConnectorType, currentConnectorID, TRAVEL);
        sleep_milli(carData.travelTime);

        if(currentConnectorType == 'N') {
            //narrow bridge
            narrowBridges[currentConnectorID] -> pass(carIndex, currentConnectorID, carData.pathObjects[i].second.second);
            narrowBridges[currentConnectorID] -> finishPass(carIndex, currentConnectorID, carData.pathObjects[i].second.second);
        }
        else if(currentConnectorType == 'F') {
            //ferry
        }
        else  {
            // crossroad

        }

    }
    

/*
    cout << "Car " << carIndex << " is processing..." << endl;
    cout << carIndex << " path length: " << carData.pathLength << endl;
    cout << carIndex << " travel time: " << carData.travelTime << endl;
    for(int i=0; i<carData.pathLength; ++i) {
        cout << carIndex << " travel time: " << carData.pathObjects[i].first << " " << 
        carData.pathObjects[i].second.first << " " << carData.pathObjects[i].second.second  << endl;
    }
*/


  

    pthread_exit(NULL);
}


int main() {

    // Take the input
    int numNarrowBridges, numFerries, numCrossroads, numCars;
    
    cin >> numNarrowBridges;
    vector<NarrowBridgeMonitor*> narrowBridges(numNarrowBridges);
    for (int i = 0; i < numNarrowBridges; ++i) {
        int travelTime, maxWaitTime;
        cin >> travelTime >> maxWaitTime;
        narrowBridges[i] = new NarrowBridgeMonitor(travelTime, maxWaitTime);
    }
    
    cin >> numFerries;
    vector<FerryMonitor*> ferries(numFerries);
    for (int i = 0; i < numFerries; ++i) {
        int travelTime, maxWaitTime, capacity;
        cin >> travelTime >> maxWaitTime >> capacity;
        ferries[i] = new FerryMonitor(travelTime, maxWaitTime, capacity);
    }
    
    cin >> numCrossroads;
    vector<CrossRoadMonitor*> crossRoads(numCrossroads);
    for (int i = 0; i < numCrossroads; ++i) {
        int travelTime, maxWaitTime;
        cin >> travelTime >> maxWaitTime;
        crossRoads[i] = new CrossRoadMonitor(travelTime, maxWaitTime);
    }
    
    cin >> numCars;
    vector<Car> cars(numCars);
    for (int i = 0; i < numCars; ++i) {
        cin >> cars[i].travelTime >> cars[i].pathLength;
        for (int j = 0; j < cars[i].pathLength; ++j) {
            string connector;
            int fromDir, toDir;
            cin >> connector >> fromDir >> toDir;
            cars[i].pathObjects.push_back(make_pair(connector, make_pair(fromDir, toDir)));
        }
    }

    vector<pthread_t> threads;
    vector<ThreadArgs*> argvector;
    for (int i = 0; i < numCars; ++i) {
        pthread_t tid;
        ThreadArgs* args = new ThreadArgs;
        args->carIndex = i;
        args->carData = cars[i];
        args->crossRoads = crossRoads;
        args->ferries = ferries;
        args->narrowBridges = narrowBridges;
        threads.push_back(tid);
        argvector.push_back(args);
    }

    InitWriteOutput();
    for(int i = 0; i < numCars; ++i) {
        pthread_create(&threads[i], NULL, carThread, (void*) argvector[i]);
    }


    for(int i = 0; i < numCars; ++i) {
        pthread_join(threads[i], NULL);
    }

    // DEALLOCATE MONITORS
    for(int i=0; i<numNarrowBridges; ++i) {
        delete narrowBridges[i];
    }
    for(int i=0; i<numCrossroads; ++i) {
        delete crossRoads[i];
    }
    for(int i=0; i<numFerries; ++i) {
        delete ferries[i];
    }

    
    return 0;
}