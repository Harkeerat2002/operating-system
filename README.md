USI - Università della Svizzera italiana, Lugano (Switzerland)
Faculty of Informatics
Spring Semester 2022
Operating System 2022
_Harkeerat Singh Sawhney_

# Operating System - Pintos Projects

This workspace contains all the OS project which uses Pintos as the base OS.

# Running the Pintos Environment for each Project

This guide will help you set up and run the Pintos environment in the OS-Projects workspace.

## Prerequisites

Ensure you have Vagrant installed on your machine. If not, you can download it from the [official Vagrant website](https://www.vagrantup.com/downloads).

## Setup

1. Navigate to the `pintos0` directory in your project. For example, if you're working on Project 1, the path would be `Project 1/pintos0/`.

```sh
cd 'Project 1/pintos0/'
```

2. Run the bootstrap.sh script. This script sets up the necessary environment for running Pintos.

```sh
./bootstrap.sh
```

3. Start the Vagrant environment.

```sh
vagrant up
```

## Usage

After setting up, you can SSH into the Vagrant environment to run Pintos.

```sh
vagrant ssh
```

Inside the Vagrant environment, navigate to the pintos directory under pintos-env.

```sh
cd pintos-env/pintos
```

Now you can run Pintos commands.

## Exiting the Vagrant Environment

To exit the Vagrant environment, run the following command.

```sh
exit
```

Remember to halt the Vagrant environment when you're done.

```sh
vagrant halt
```
