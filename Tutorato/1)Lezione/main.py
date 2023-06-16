class Node:
    def __init__(root, key):
        root.key = key
        root.left = None
        root.right = None

def insert(node, key):
    if node is None:
        return Node(key)
    if key < node.key:
        node.left = insert(node.left, key)
    else:
        node.right = insert(node.right, key)
    return node

def search(node, key):
    if node is None or node.key == key:
        return node
    if key < node.key:
        return search(node.left, key)
    return search(node.right, key)

def inOrder(node):
    if node is not None:
        inOrder(node.left)
        print(node.key)
        inOrder(node.right)

# Esempio di utilizzo
root = None
root = insert(root, 8)
root = insert(root, 3)
root = insert(root, 10)
root = insert(root, 1)
root = insert(root, 6)
root = insert(root, 14)
root = insert(root, 4)
root = insert(root, 7)
root = insert(root, 13)

print("Visita inOrder:")
inOrder(root)
