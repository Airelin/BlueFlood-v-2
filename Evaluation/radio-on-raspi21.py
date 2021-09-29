#!/usr/bin/env python3
from sys import path
from sys import argv

directory = 'Round-nr_Fixed_run2/5366_round_fixed-new-blueflood-mode6/'
raspis = {'raspi01/', 'raspi02/', 'raspi03/', 'raspi04/', 'raspi05/', 'raspi06/', 'raspi07/', 'raspi08/', 'raspi09/', 
              'raspi10/', 'raspi12/', 'raspi13/', 'raspi14/', 'raspi15/', 'raspi16/', 'raspi17/', 'raspi18/', 'raspi19/', 
              'raspi20/', 'raspi21/'}



def all_raspis_radio():
    for pi in raspis:
        radio_on_time(pi, directory)

def radio_on_time(raspi, directory):

    #paths
    path_to_origin='Logs/'+ directory +'logs/'+ raspi +'log.txt'
    path_to_cleaned='Logs/'+ directory +'logs/'+ raspi +'cleaned.txt'
    path_to_between='Logs/'+ directory +'logs/'+ raspi +'between.txt'
    path_to_data='Logs/'+ directory +'logs/'+ raspi +'data.txt'

    # opens the specified file
    with open(path_to_origin, encoding="iso-8859-14") as p:
        lines = p.readlines()

    # gets the lines containing the string defined in line.find(string)
    file = ""
    for line in lines:
        if(line.find('radio')>0):
            file = file + line
        
    # writes file to specified path
    with open(path_to_cleaned,'w') as f_clean:
        f_clean.write(file)

    # make inbetween file
    file3=""
    with open(path_to_cleaned,'r') as f_cleaned:
        l_cleaned = f_cleaned.readlines()
        count = 0
        for line in l_cleaned:
            if(count==0 and line.find('off')):
                if(count>0):
                    file3 = file3 + line
            else:
                if(count>1):
                    file3 = file3 + line
            count+=1

    with open(path_to_between,'w') as f_between:
        f_between.write(file3)

    file4=""
    with open(path_to_between,'r') as f_b:
        l_b = f_b.readlines()
        for line in l_b:
            b = 0
            for j in range (2,len(line.split())):
                if(not(b) and not(line.split()[j].startswith('0'))):
                    if(line.split()[j].endswith(',')):
                        file4 = file4 + line.split()[j] + ' '
                        b = 1
                    else:
                        file4 = file4 + line.split()[j] + '\n'

    #write to new file
    with open(path_to_between,'w') as f_between:
        f_between.write(file4)

    file5 = ""
    with open(path_to_between,'r') as f_b:
        l_b = f_b.readlines()
        for line in l_b:
            if(len(line.split())>1):
                on = int(line.split(', ')[0], base=10)
                off = int(line.split(', ')[1], base=10)
                time1 = off-on
                time2 = time1/16000
                if(time2 >= 0):
                    file5 = file5 + str(time2) + '\n'

    #write to new file
    with open(path_to_data,'w') as f_data:
        f_data.write(file5)

def append_files():
    file_big = ""
    path_to_big_file='Logs/'+ directory +'logs/radio-on.txt'
    for raspi in raspis:
        path_to_data='Logs/'+ directory +'logs/'+ raspi +'data.txt'
        with open(path_to_data,'r') as file:
            lines = file.readlines()
            for line in lines:
                file_big = file_big + line
    
    with open(path_to_big_file,'w') as big_file:
        big_file.write(file_big)

if __name__ == "__main__":
        all_raspis_radio()
        append_files()