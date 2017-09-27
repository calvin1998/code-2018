import serial

//read serial port from Teensy

[][] data //store incoming data, voltage/temp/current

//figure setup
create figure
axis, limits, etc

while available
    read serial port
    parse package and add to data if new received
        -compare with "key" characters to get correct variable type

    if (new data point was added) {
        update plot
    } //not necessary

end


function plot([] newDataThatWasAddedSinceLastPlot)
//array... of new data added since last plot
update plot
if



