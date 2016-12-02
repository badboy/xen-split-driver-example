# Example for a split driver for the Xen platform

Drivers targetting Xen domains use a split-driver model, where one end is placed in the driver domain (typically `dom0`)
 and the other end is placed in an unprivileged domain (like `domU`).

However, the exact details on how to write a driver are not really documented and easy to find,
so I decided to write this up as I go along.
Detailed explanations will be posted to [my blog](https://fnordig.de/) and this repository will contain the code.

## Status

This is a work in progress.

Starting in November 2016, I am writing my master thesis, where I have to work with Xen.
Over the time of this work I hope to complete posts on individual parts of the driver development.

## Content

1. [Enabling the driver](https://fnordig.de/2016/12/02/xen-a-backend-frontend-driver-example/)
1. Initial communication (tbd)
1. Event channel setup (tbd)
1. Ring communication (tbd)
1. Sharing memory using grants (tbd)
