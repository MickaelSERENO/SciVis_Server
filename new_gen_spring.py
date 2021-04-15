#!/usr/bin/python3
'''
Program for constructing a dataset consisting of a ball inside a hemisphere.
'''

from math import *
from random import *
from struct import *
import sys

basename = "spring"

f = open(basename + '.cp', 'wb')

unselected_id = 0
selected_id = 1

string_radius = 0.5

num_particles_spring = 100000
num_particles_noise = 40000

def uniform_rectangular_shell(number, center=(0.0,0.0,0.0), semiaxes=(0.5, 0.5, 0.5),
                             min_r=0.0, max_r=1.0):
  x0 = center[0]
  y0 = center[1]
  z0 = center[2]
  a = semiaxes[0]
  b = semiaxes[1]
  c = semiaxes[2]
  counter = 0
  while counter < number:
    x = uniform(x0 - a, x0 + a)
    y = uniform(y0 - b, y0 + b)
    z = uniform(z0 - c, z0 + c)

    yield x, y, z
    counter += 1

#formula: https://en.wikipedia.org/wiki/Spring_(mathematics)
def spring(number, beg=(0.0, 0.0, -0.5), small_radius=0.10, big_radius=0.25, length=1.0, nb_round=3):
  x0 = beg[0]
  y0 = beg[1]
  z0 = beg[2]

  P = (length-small_radius)/(2*nb_round) #Ensure length

  counter = 0
  while counter < number:
    v = uniform(0, pi)
    u = uniform(0, 2*nb_round*pi)

    x = (big_radius + small_radius*cos(v))*cos(u)
    y = (big_radius + small_radius*cos(v))*sin(u)
    z = small_radius * sin(v) + P*u/pi

    yield x, y, z
    counter += 1

#Number of points:
f.write(pack('I', num_particles_noise + num_particles_spring))

#Noise
for x,y,z in uniform_rectangular_shell(num_particles_noise):
  f.write(pack('!fff', x, y, z))

#Spring
for x,y,z in spring(num_particles_spring):
  f.write(pack('!fff', x, y, z))

#Noise:
for i in range(num_particles_noise):
    f.write(pack('!f', 0))

#Spring
for i in range(num_particles_spring):
    f.write(pack('!f', 1))

f.close()
